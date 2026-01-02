/*
 * Copyright (c) 2025-2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <hardware/sync.h>
#include "pico/cyw43_arch.h"
#include "mqtt_cu_internal.h"
#include "lwip/dns.h"

static inline uint32_t md_task_list_lock(void) {
        return save_and_disable_interrupts();
}

static inline void md_task_list_unlock(uint32_t status) {
        restore_interrupts(status);
}

static pch_devib_t *task_head = NULL;
static pch_devib_t *task_tail = NULL;

volatile static bool ready_for_tasks = false;

static inline bool task_list_active(void) {
        return ready_for_tasks;
}

static void task_list_pause(void) {
        ready_for_tasks = false;
}

static void task_list_restart(void) {
        md_cu_statistics.task_restart++;
        ready_for_tasks = true;
}

static void task_list_pop(void) {
        uint32_t status = md_task_list_lock();

        pch_devib_t *old_task_head = task_head;
        task_head = md_get_next_task(task_head);
        md_set_next_task(old_task_head, NULL);
        if (!task_head)
                task_tail = NULL;

        md_task_list_unlock(status);
}

void md_task_list_append(pch_devib_t *devib) {
        uint32_t status = md_task_list_lock();
        if (task_tail) {
                md_set_next_task(task_tail, devib);
                task_tail = devib;
        } else {
                task_head = devib;
                task_tail = devib;
        }

        md_task_list_unlock(status);
        task_list_restart();
}

static void md_task_result(pch_devib_t *devib, err_t err, bool serial_release) {
        if (err == ERR_MEM) {
                // Pause tasks and add this task again to retry later
                task_list_pause();
                md_task_list_append(devib);
                return;
        }

        if (serial_release)
                md_serial_release(devib);

        if (err == ERR_OK)
                pch_hldev_end_ok(devib);
        else
                pch_hldev_end_intervention(devib, -err);
}

// task requested by CCW SUBSCRIBE or UNSUBSCRIBE

static void sub_unsub_request_cb(void *arg, err_t err) {
        pch_devib_t *devib = arg;
        md_task_result(devib, err, false);
}

static bool task_try_sub_unsub(pch_devib_t *devib, uint8_t sub) {
        tmbuf_t *tm = get_current_tmbuf_required(devib);
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        err_t err = mqtt_sub_unsub(&cfg->client, tmbuf_topic_ptr(tm),
                0, sub_unsub_request_cb, devib, sub);
        return err != ERR_MEM;
}

// task requested by CCW PUBLISH

static void pub_request_cb(void *arg, err_t err) {
        pch_devib_t *devib = arg;
        md_task_result(devib,err, false);

        // The completion of this request may have freed up some
        // memory so we can retry any pending tasks that failed due
        // to ERR_MEM.
        task_list_restart();
}

static bool task_try_publish(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_required(devib);
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        err_t err = mqtt_publish(&cfg->client, tmbuf_topic_ptr(tm),
                tmbuf_message_ptr(tm), tm->mlen, 0, 0, pub_request_cb,
                devib);
        return err != ERR_MEM;
}

// task requested by CCW CONNECT

static void connection_status_cb(mqtt_client_t *c, void *arg, mqtt_connection_status_t status) {
        pch_devib_t *devib = arg;
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        err_t err;

        printf("MQTT connection status changed to %d\n", status);
        bool was_conn_status_ready = md_is_conn_status_ready(cfg);
        md_set_conn_status_ready(cfg, true);
        if (was_conn_status_ready)
                return;

        if (status == MQTT_CONNECT_ACCEPTED) {
                err = ERR_OK;
                printf("connected to MQTT successfully\n");
                mqtt_set_inpub_callback(&cfg->client,
                        md_inpub_start_cb, md_inpub_data_cb, cfg);
        } else {
                printf("MQTT connection failed, status=%d\n", status);
                err = ERR_CONN; // means "NOT connected"
                // TODO propagate detailed failure status
        }

        md_task_result(devib, err, true);
}

static void connect_dns_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
        pch_devib_t *devib = arg;

        printf("Connecting to MQTT server at IP address %s\n",
                ip4addr_ntoa(ipaddr));

        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        cfg->mqtt_ipaddr = *ipaddr;

        struct mqtt_connect_client_info_t ci = {
                .client_id = cfg->mqtt_client_id,
                .client_user = cfg->mqtt_username,
                .client_pass = cfg->mqtt_password
        };

        uint16_t port = cfg->mqtt_port;
        if (port == 0)
                port = DEFAULT_MQTT_PORT;

        mqtt_client_connect(&cfg->client, ipaddr, port,
                connection_status_cb, devib, &ci);
}

static bool task_try_connect(pch_devib_t *devib) {
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        printf("running DNS query if needed for MQTT server %s\n",
                cfg->mqtt_hostname);

        if (cfg->mqtt_ipaddr.addr == 0) {
                err_t err = dns_gethostbyname(cfg->mqtt_hostname,
                        &cfg->mqtt_ipaddr, connect_dns_cb, devib);
                if (err == ERR_MEM)
                        return false; // will retry later
                if (err == ERR_INPROGRESS) {
                        printf("DNS lookup in progress...\n");
                        return true; // connect_dns_cb will progress
                }
                if (err != ERR_OK)
                        md_task_result(devib, err, true);
        }

        printf("no need to wait for DNS\n");
        // tail-call DNS callback immediately
        connect_dns_cb(cfg->mqtt_hostname, &cfg->mqtt_ipaddr, devib);
        return true; // connect_done_cb will progress
}

// task_try() returns true if the task running should continue with
// running tasks on the list. Returning false (typically when an
// MQTT API function returns ERR_MEM) causes the task list to be
// suspended until restarted with the same task when an incoming
// packet arrives (and so may have freed up memory).
static bool task_try(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        uint8_t ccwcmd = md->hldev.ccwcmd;

        switch (ccwcmd) {
        case CMD(PUBLISH):
                return task_try_publish(devib);

        case CMD(SUBSCRIBE):
                return task_try_sub_unsub(devib, 1);

        case CMD(UNSUBSCRIBE):
                return task_try_sub_unsub(devib, 0);

        case CMD(CONNECT):
                return task_try_connect(devib);
        }

        panic("unimplemented task for CCW %02x", ccwcmd);
}

void mqtt_cu_poll(void) {
        cyw43_arch_poll();

        while (task_list_active() && task_head) {
                if (task_try(task_head)) {
                        md_cu_statistics.task_success++;
                        task_list_pop();
                } else {
                        md_cu_statistics.task_pause++;
                        task_list_pause();
                }

                cyw43_arch_poll();
        }
}
