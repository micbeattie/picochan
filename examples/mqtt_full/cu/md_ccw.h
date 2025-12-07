/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _MQTT_MD_CCW_H
#define _MQTT_MD_CCW_H

void md_ccw_write_topic(pch_devib_t *devib);
void md_ccw_write_message(pch_devib_t *devib);
void md_ccw_write_message_append(pch_devib_t *devib);
void md_ccw_start_task_with_current_tmbuf(pch_devib_t *devib);

void md_ccw_read_topic(pch_devib_t *devib);
void md_ccw_read_message(pch_devib_t *devib);
void md_ccw_read_topic_and_message(pch_devib_t *devib);

void md_ccw_connect(pch_devib_t *devib);
void md_ccw_set_current_id(pch_devib_t *devib);

void md_ccw_set_filter_id(pch_devib_t *devib);
void md_ccw_get_ring(pch_devib_t *devib);
void md_ccw_set_ring(pch_devib_t *devib);
void md_ccw_start_ring(pch_devib_t *devib);
void md_ccw_stop_ring(pch_devib_t *devib);
void md_ccw_wait(pch_devib_t *devib);
void md_ccw_ack(pch_devib_t *devib);

void md_ccw_connect(pch_devib_t *devib);
void md_ccw_disconnect(pch_devib_t *devib);
void md_ccw_set_mqtt_hostname(pch_devib_t *devib);
void md_ccw_set_mqtt_username(pch_devib_t *devib);
void md_ccw_set_mqtt_password(pch_devib_t *devib);
void md_ccw_set_mqtt_username(pch_devib_t *devib);
void md_ccw_set_mqtt_client_id(pch_devib_t *devib);
void md_ccw_set_mqtt_port(pch_devib_t *devib);

#endif
