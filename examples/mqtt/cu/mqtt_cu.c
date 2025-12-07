/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <string.h>

#include "pico/stdlib.h"
#include "pico/status_led.h"
#include "pico/cyw43_arch.h"
#include "hardware/sync.h"

#include "lwip/tcp.h"

#include "lwip/altcp_tcp.h"
#include "lwip/apps/mqtt.h"

#include "lwip/apps/mqtt_priv.h"

#include "picochan/hldev.h"
#include "picochan/ccw.h"

#include "../md_api.h"
#include "mqtt_util.h"

#ifndef MAX_NUM_MQTT_DEVS
#define MAX_NUM_MQTT_DEVS 8
#endif

#define MQTT_ENABLE_TRACE true

#ifndef TOPIC_BUF_SIZE
#define TOPIC_BUF_SIZE 256
#endif

static_assert(TOPIC_BUF_SIZE >= 1  && TOPIC_BUF_SIZE <= 65535,
        "TOPIC_BUF_SIZE must be between 1 and 65535");

// topic buffer must have room for trailing \0
#define MAX_TOPIC_LEN (TOPIC_BUF_SIZE-1)

#ifndef MESSAGE_BUF_SIZE
#define MESSAGE_BUF_SIZE 1024
#endif

// message buffer does not use a trailing \0
#define MAX_MESSAGE_LEN MESSAGE_BUF_SIZE

typedef struct mqtt_dev {
        pch_hldev_t             hldev; // must be first field
        struct mqtt_dev         *next_pub;
        uint16_t                topic_len;
        uint16_t                message_len;
        char                    topic[TOPIC_BUF_SIZE];
        char                    message[MESSAGE_BUF_SIZE];
} mqtt_dev_t;

static_assert(offsetof(mqtt_dev_t, hldev) == 0,
        "hldev must be first field in mqtt_dev_t");

mqtt_dev_t      mqtt_devs[MAX_NUM_MQTT_DEVS];
mqtt_client_t   client;
bool            ready_to_try_publish;
mqtt_dev_t      *md_pub_head;
mqtt_dev_t      *md_pub_tail;

static pch_hldev_t *md_get_hldev(pch_hldev_config_t *hdcfg, int i) {
        return &mqtt_devs[i].hldev;
}

static void md_hldev_callback(pch_devib_t *devib);

pch_hldev_config_t mqtt_hldev_config = {
        .get_hldev = md_get_hldev,
        .start = md_hldev_callback
};

static pch_devib_t *md_get_devib(mqtt_dev_t *md) {
        int i = md - mqtt_devs;
        return pch_hldev_get_devib(&mqtt_hldev_config, i);
}

static inline uint32_t md_pub_list_lock(void) {
        return save_and_disable_interrupts();
}
        
static inline void md_pub_list_unlock(uint32_t status) {
        restore_interrupts(status);
}

static void md_end(pch_devib_t *devib, err_t err) {
        if (err == ERR_OK)
                pch_hldev_end_ok(devib);
        else
                pch_hldev_end_intervention(devib, err);
}

static void mqtt_pub_start_cb(void *arg, const char *topic, u32_t tot_len) {
        // ignore incoming topic - it will be pico/command
}

static void mqtt_pub_data_cb(void *arg, const u8_t *data, uint16_t len, uint8_t flags) {
        if (len == 4 && !memcmp(data, "led1", 4))
                status_led_set_state(true);
        else if (len == 4 && !memcmp(data, "led0", 4))
                status_led_set_state(false);
        else
                printf("unknown command: %*s\n", len, data);
}

#define MQTT_CLIENT_ID "pico"

bool mqtt_connect_cu_sync(const char *mqtt_server_host, uint16_t mqtt_server_port, const char *mqtt_username, const char *mqtt_password) {
        printf("connecting to MQTT server...\n");

        ip_addr_t addr;
        if (!dns_lookup_sync(mqtt_server_host, &addr)) {
                printf("dns lookup failed\n");
                return false;
        }

        struct mqtt_connect_client_info_t ci = {
                .client_id = MQTT_CLIENT_ID,
                .client_user = mqtt_username,
                .client_pass = mqtt_password
        };
        if (!mqtt_connect_sync(&client, &addr, mqtt_server_port, &ci)) {
                printf("mqtt connect failed\n");
                return false;
        }

        printf("connected to MQTT server\n");
        mqtt_set_inpub_callback(&client,
                mqtt_pub_start_cb, mqtt_pub_data_cb, NULL);
        subscribe_sync(&client, MQTT_CLIENT_ID "/command");

        return true;
}

// CCW command implementations

// Called when a MD_CCW_CMD_SET_TOPIC CCW has received all the
// data available
static void md_set_topic_received(pch_devib_t *devib) {
        mqtt_dev_t *md = (mqtt_dev_t *)pch_hldev_get(devib);
        md->topic_len = md->hldev.count;
        // append trailing \0 - guaranteed room since
        // MAX_TOPIC_LEN is MAX_TOPIC_BUF_SIZE-1
        md->topic[md->topic_len] = '\0';
        pch_hldev_end_ok(devib);
}

// Called for a MD_CCW_CMD_SET_TOPIC CCW
static void md_set_topic_init(pch_devib_t *devib) {
        mqtt_dev_t *md = (mqtt_dev_t *)pch_hldev_get(devib);
        md->topic_len = 0;
        pch_hldev_receive_then(devib, md->topic, MAX_TOPIC_LEN,
                md_set_topic_received);
}

static void append_to_pub_list(mqtt_dev_t *md) {
        uint32_t status = md_pub_list_lock();
        if (md_pub_tail) {
                md_pub_tail->next_pub = md;
                md_pub_tail = md;
        } else {
                md_pub_head = md;
                md_pub_tail = md;
        }

        md_pub_list_unlock(status);
}

static void pop_from_pub_list(void) {
        uint32_t status = md_pub_list_lock();
        md_pub_head = md_pub_head->next_pub;
        md_pub_head->next_pub = NULL;
        if (!md_pub_head)
                md_pub_tail = NULL;

        md_pub_list_unlock(status);
}

static void md_pub_request_cb(void *arg, err_t err) {
        pch_devib_t *devib = (pch_devib_t *)arg;

        md_end(devib, err);

        // The completion of this request probably freed up some
        // memory so we can retry any pending publishes that
        // failed due to ERR_MEM.
        ready_to_try_publish = true;
}

// returns true if the publish completed either successfully or
// with a permanent error. Returns false if the publish returned
// ERR_MEM meaning that it should be retried after more memory
// becomes available (after incoming TCP ACKs let lwIP free up
// space in outgoing TCP buffers).
static bool md_try_publish(mqtt_dev_t *md) {
        pch_devib_t *devib = md_get_devib(md);
        err_t err = mqtt_publish(&client, md->topic, md->message,
                md->message_len, 0, 0, md_pub_request_cb, devib);

        if (err == ERR_MEM)
                return false; // not enough memory to publish

        if (err != ERR_OK)
                md_end(devib, err);

        // if err == ERR_OK, the pub callback does the pch_hldev_end()
        return true;
}

// Called when a PCH_CCW_CMD_WRITE CCW has received all the
// data available
static void md_publish_received(pch_devib_t *devib) {
        mqtt_dev_t *md = (mqtt_dev_t *)pch_hldev_get(devib);
        md->message_len = md->hldev.count;
        append_to_pub_list(md);
        pch_hldev_end_ok(devib);
}

// Called for a PCH_CCW_CMD_WRITE CCW
static void md_publish_init(pch_devib_t *devib) {
        mqtt_dev_t *md = (mqtt_dev_t *)pch_hldev_get(devib);
        md->message_len = 0;
        pch_hldev_receive_then(devib, md->message,
                sizeof(md->message), md_publish_received);
}

static void md_hldev_callback(pch_devib_t *devib) {
        uint8_t ccwcmd = devib->payload.p0;
        switch (ccwcmd) {
        case PCH_CCW_CMD_WRITE:
                md_publish_init(devib);
                break;

        case MD_CCW_CMD_SET_TOPIC:
                md_set_topic_init(devib);
                break;

        default:
                pch_hldev_end_reject(devib, EINVALIDCMD);
                break;
        }
}

void mqtt_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices) {
        assert(num_devices <= MAX_NUM_MQTT_DEVS);
        pch_hldev_config_init(&mqtt_hldev_config, cu, first_ua, num_devices);
        memset(mqtt_devs, 0, sizeof(mqtt_devs));
        ready_to_try_publish = true;
}

void mqtt_cu_poll(void) {
        cyw43_arch_poll();
        while (ready_to_try_publish && md_pub_head) {
                if (md_try_publish(md_pub_head))
                        pop_from_pub_list();
                else
                        ready_to_try_publish = false;

                // cyw43_arch_poll();
        }
}
