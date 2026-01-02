/*
 * Copyright (c) 2025-2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "mqtt_cu_internal.h"
#include "md_ccw.h"

static bool md_serial_acquire(pch_devib_t *devib) {
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        if (cfg->md_serial) {
                pch_hldev_end_reject(devib, MD_ERR_CU_BUSY);
                return false;
        }

        cfg->md_serial = get_mqtt_dev(devib);
        return true;
}

void md_serial_release(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        assert(md == NULL || md == cfg->md_serial);
        cfg->md_serial = NULL;
}

void md_ccw_connect(pch_devib_t *devib) {
       if (!md_serial_acquire(devib))
                return;

        md_task_list_append(devib);
}

// Called to do a DISCONNECT
void md_ccw_disconnect(pch_devib_t *devib) {
        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        mqtt_disconnect(&cfg->client);
        pch_hldev_end_ok(devib);
}

void end_serialised(pch_devib_t *devib) {
        pch_hldev_end_ok(devib);
        md_serial_release(devib);
}

void end_serialised_receive_string(pch_devib_t *devib) {
        pch_hldev_terminate_string(devib);
        pch_hldev_end_ok(devib);
        md_serial_release(devib);
}

// Called to start a SET_MQTT_HOSTNAME CCW
void md_ccw_set_mqtt_hostname(pch_devib_t *devib) {
        if (!md_serial_acquire(devib))
                return;

        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        pch_hldev_receive_then(devib, cfg->mqtt_hostname,
                sizeof(cfg->mqtt_hostname)-1,
                end_serialised_receive_string);
}

// Called to start a SET_MQTT_USERNAME CCW
void md_ccw_set_mqtt_username(pch_devib_t *devib) {
        if (!md_serial_acquire(devib))
                return;

        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        pch_hldev_receive_then(devib, cfg->mqtt_username,
                sizeof(cfg->mqtt_username)-1,
                end_serialised_receive_string);
}

// Called to start a SET_MQTT_PASSWORD CCW
void md_ccw_set_mqtt_password(pch_devib_t *devib) {
        if (!md_serial_acquire(devib))
                return;

        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        pch_hldev_receive_then(devib, cfg->mqtt_password,
                sizeof(cfg->mqtt_password)-1,
                end_serialised_receive_string);
}

// Called to start a SET_MQTT_CLIENT_ID CCW
void md_ccw_set_mqtt_client_id(pch_devib_t *devib) {
        if (!md_serial_acquire(devib))
                return;

        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        pch_hldev_receive_then(devib, cfg->mqtt_client_id,
                sizeof(cfg->mqtt_client_id)-1,
                end_serialised_receive_string);
}

// Called to start a SET_MQTT_PORT CCW
void md_ccw_set_mqtt_port(pch_devib_t *devib) {
        if (!md_serial_acquire(devib))
                return;

        mqtt_cu_config_t *cfg = get_mqtt_cu_config(devib);
        cfg->mqtt_port = 0;
        pch_hldev_receive_then(devib, &cfg->mqtt_port,
                sizeof(cfg->mqtt_port), end_serialised);
}
