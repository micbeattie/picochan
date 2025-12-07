/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _MQTT_CU_H
#define _MQTT_CU_H

#include <string.h>

#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"

#include "picochan/hldev.h"
#include "picochan/ccw.h"

#include "../mqtt_cu_api.h"
#include "md_tmbuf.h"

#ifndef NUM_TMBUF_BUFFERS
#define NUM_TMBUF_BUFFERS       64
#endif

static_assert(NUM_TMBUF_BUFFERS >= 1 && NUM_TMBUF_BUFFERS <= 32767,
        "NUM_TMBUF_BUFFERS must be between 1 and 32767");

static_assert(NUM_MQTT_DEVS >= 1 && NUM_MQTT_DEVS <= 256,
        "NUM_MQTT_DEVS must be between 1 and 256");

static_assert(MQTT_TOPIC_MAXLEN >= 0 && MQTT_TOPIC_MAXLEN <= 65535,
        "MQTT_TOPIC_MAXLEN must be between 0 and 65535");
#define MQTT_TOPIC_BUFFSIZE  (MQTT_TOPIC_MAXLEN+1)

static_assert(MQTT_MESSAGE_MAXLEN >= 0 && MQTT_MESSAGE_MAXLEN <= 65535,
        "MQTT_MESSAGE_MAXLEN must be between 0 and 65535");
// message buffer does not need a trailing \0

#define stringify(s) # s

#define CMD(suffix) MQTT_CCW_CMD_ ## suffix

typedef struct mqtt_dev {
        pch_hldev_t     hldev; // must be first field
        pch_devib_t     *next_task;
        md_ring_t       ring;
        uint16_t        cur;
        uint16_t        filt;
        uint8_t         flags;
} mqtt_dev_t;

#define MD_FLAG_RING_STARTED    0x01
#define MD_FLAG_RING_OVERFLOW   0x02

static inline bool md_ring_is_started(mqtt_dev_t *md) {
        return md->flags & MD_FLAG_RING_STARTED;
}

static inline void md_set_ring_is_started(mqtt_dev_t *md, bool b) {
        if (b)
                md->flags |= MD_FLAG_RING_STARTED;
        else
                md->flags &= ~MD_FLAG_RING_STARTED;
}

static inline bool md_ring_is_overflow(mqtt_dev_t *md) {
        return md->flags & MD_FLAG_RING_OVERFLOW;
}

static inline void md_set_ring_is_overflow(mqtt_dev_t *md, bool b) {
        if (b)
                md->flags |= MD_FLAG_RING_OVERFLOW;
        else
                md->flags &= ~MD_FLAG_RING_OVERFLOW;
}

static_assert(offsetof(mqtt_dev_t, hldev) == 0,
        "hldev must be first field in mqtt_dev_t");

static_assert(MQTT_HOSTNAME_MAXLEN >= 0 && MQTT_HOSTNAME_MAXLEN <= 255,
        "MQTT_HOSTNAME_MAXLEN must be between 0 and 255");
#define MQTT_HOSTNAME_BUFFSIZE  (MQTT_HOSTNAME_MAXLEN+1)

static_assert(MQTT_USERNAME_MAXLEN >= 0 && MQTT_USERNAME_MAXLEN <= 65535,
        "MQTT_USERNAME_MAXLEN must be between 0 and 65535");
#define MQTT_USERNAME_BUFFSIZE  (MQTT_USERNAME_MAXLEN+1)

static_assert(MQTT_PASSWORD_MAXLEN >= 0 && MQTT_PASSWORD_MAXLEN <= 65535,
        "MQTT_PASSWORD_MAXLEN must be between 0 and 65535");
#define MQTT_PASSWORD_BUFFSIZE  (MQTT_PASSWORD_MAXLEN+1)

static_assert(MQTT_CLIENT_ID_MAXLEN >= 0 && MQTT_CLIENT_ID_MAXLEN <= 65535,
        "MQTT_CLIENT_ID_MAXLEN must be between 0 and 65535");
#define MQTT_CLIENT_ID_BUFFSIZE  (MQTT_CLIENT_ID_MAXLEN+1)

typedef struct mqtt_cu_config {
        pch_hldev_config_t      hldev_config; // must be first
        mqtt_client_t           client;
        mqtt_dev_t              *md_serial; // active serialised chan prog
        uint8_t                 flags;
        char                    mqtt_hostname[MQTT_HOSTNAME_BUFFSIZE];
        uint16_t                mqtt_port;
        ip_addr_t               mqtt_ipaddr; // resolved from mqtt_hostname
        char                    mqtt_username[MQTT_USERNAME_BUFFSIZE];
        char                    mqtt_password[MQTT_PASSWORD_BUFFSIZE];
        char                    mqtt_client_id[MQTT_CLIENT_ID_BUFFSIZE];
        mqtt_dev_t              mds[NUM_MQTT_DEVS];
        tmbuf_t                 tmbufs[NUM_TMBUF_BUFFERS];
} mqtt_cu_config_t;

static_assert(offsetof(mqtt_cu_config_t, hldev_config) == 0,
        "hldev_config must be first field in mqtt_cu_config_t");

// values of mqtt_cu_config_t flags
#define MD_CU_CONN_STATUS_READY         0x01

static inline bool md_is_conn_status_ready(mqtt_cu_config_t *cfg) {
        return cfg->flags & MD_CU_CONN_STATUS_READY;
}

static inline void md_set_conn_status_ready(mqtt_cu_config_t *cfg, bool b) {
        if (b)
                cfg->flags |= MD_CU_CONN_STATUS_READY;
        else
                cfg->flags &= ~MD_CU_CONN_STATUS_READY;
}

static inline mqtt_dev_t *get_mqtt_dev(pch_devib_t *devib) {
        return (mqtt_dev_t *)pch_hldev_get(devib);
}

static inline pch_devib_t *md_get_next_task(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        return md->next_task;
}

static inline pch_devib_t *md_set_next_task(pch_devib_t *devib, pch_devib_t *next_task) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        pch_devib_t *old_next_task = md->next_task;
        md->next_task = next_task;
        return old_next_task;
}

static inline mqtt_cu_config_t *get_mqtt_cu_config(pch_devib_t *devib) {
        return (mqtt_cu_config_t *)pch_hldev_get_config(devib);
}

static inline tmbuf_t *get_tmbuf_nocheck(mqtt_cu_config_t *cfg, uint16_t id) {
        return &cfg->tmbufs[id];
}

static inline tmbuf_t *get_tmbuf(mqtt_cu_config_t *cfg, uint16_t id) {
        if (id >= NUM_TMBUF_BUFFERS)
                return NULL;

        return get_tmbuf_nocheck(cfg, id);
}

static inline tmbuf_t *get_tmbuf_required(mqtt_cu_config_t *cfg, uint16_t id) {
        if (id >= NUM_TMBUF_BUFFERS)
                panic("bad tmbuf id");

        return get_tmbuf_nocheck(cfg, id);
}

static inline pch_devib_t *md_get_devib(mqtt_cu_config_t *cfg, mqtt_dev_t *md) {
        int i = md - cfg->mds;
        pch_hldev_config_t *hdcfg = &cfg->hldev_config;
        assert(i >= 0 && i < hdcfg->dev_range.num_devices);
        return pch_hldev_get_devib(hdcfg, i);
}

static inline tmbuf_t *get_current_tmbuf_required(pch_devib_t *devib) {
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        mqtt_dev_t *md = get_mqtt_dev(devib);
        return get_tmbuf_required(cfg, md->cur);
}

static inline tmbuf_t *get_tmbuf_or_reject(pch_devib_t *devib, uint16_t id) {
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        tmbuf_t *tm = get_tmbuf(cfg, id);
        if (!tm)
                pch_hldev_end_reject(devib, MD_ERR_INVALID_TMBUF);

        return tm;
}

static inline tmbuf_t *get_current_tmbuf_or_reject(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        return get_tmbuf_or_reject(devib, md->cur);
}

static inline uint16_t md_ring_increment(md_ring_t *mr, uint16_t n) {
        assert(md_ring_valid(mr));
        n++;
        if (n >= mr->end)
                n = mr->start;

        return n;
}

// md_ring_lock()/md_ring_lock() protect against race-sensitive
// changes from ACK, WAIT and incoming messages operating on an
// mqtt_dev's ring buffer. A global disable/enable IRQs suffices.
static inline uint32_t md_ring_lock(void) {
        return save_and_disable_interrupts();
}

static inline void md_ring_unlock(uint32_t status) {
        restore_interrupts(status);
}

void md_task_list_append(pch_devib_t *devib);
void md_serial_release(pch_devib_t *devib);
void md_wake(mqtt_cu_config_t *cfg, mqtt_dev_t *md);

void md_inpub_start_cb(void *arg, const char *topic, u32_t tot_len);
void md_inpub_data_cb(void *arg, const u8_t *data, uint16_t len, uint8_t flags);

void md_hldev_callback(pch_devib_t *devib);

extern md_cu_stats_t md_cu_statistics;

#endif
