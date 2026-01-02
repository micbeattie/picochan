/*
 * Copyright (c) 2025-2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "mqtt_cu_internal.h"
#include "md_ccw.h"

pch_devib_callback_t md_ccw_callbacks[] = {
	[CMD(READ_TOPIC_AND_MESSAGE)] = md_ccw_read_topic_and_message,
	[CMD(READ_MESSAGE)] = md_ccw_read_message,
	[CMD(READ_TOPIC)] = md_ccw_read_topic,
	[CMD(WAIT)] = md_ccw_wait,
	[CMD(ACK)] = md_ccw_ack,
	[CMD(GET_RING)] = md_ccw_get_ring,
	[CMD(SUBSCRIBE)] = md_ccw_start_task_with_current_tmbuf,
	[CMD(UNSUBSCRIBE)] = md_ccw_start_task_with_current_tmbuf,
	[CMD(PUBLISH)] = md_ccw_start_task_with_current_tmbuf,
	[CMD(CONNECT)] = md_ccw_connect,
	[CMD(DISCONNECT)] = md_ccw_disconnect,
	[CMD(START_RING)] = md_ccw_start_ring,
	[CMD(STOP_RING)] = md_ccw_stop_ring,
	[CMD(SET_CURRENT_ID)] = md_ccw_set_current_id,
	[CMD(SET_FILTER_ID)] = md_ccw_set_filter_id,
	[CMD(WRITE_TOPIC)] = md_ccw_write_topic,
	[CMD(WRITE_MESSAGE)] = md_ccw_write_message,
	[CMD(WRITE_MESSAGE_APPEND)] = md_ccw_write_message_append,
	[CMD(SET_RING)] = md_ccw_set_ring,
	// [CMD(MATCH_MESSAGE)] = md_ccw_match_message,
	[CMD(SET_MQTT_HOSTNAME)] = md_ccw_set_mqtt_hostname,
	[CMD(SET_MQTT_PORT)] = md_ccw_set_mqtt_port,
	[CMD(SET_MQTT_USERNAME)] = md_ccw_set_mqtt_username,
	[CMD(SET_MQTT_PASSWORD)] = md_ccw_set_mqtt_password,
	[CMD(SET_MQTT_CLIENT_ID)] = md_ccw_set_mqtt_client_id
};

// Main CCW callback from hldev
void md_hldev_callback(pch_devib_t *devib) {
        uint8_t ccwcmd = devib->payload.p0;

        // Test tracing
#define MD_TRC_RT_HLDEV_CB 200
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        pch_cus_trace_write_user(MD_TRC_RT_HLDEV_CB,
                ((uint8_t[2]){ua, ccwcmd}), 2);

        pch_devib_callback_t callback = NULL;
        if (ccwcmd < count_of(md_ccw_callbacks))
                callback = md_ccw_callbacks[ccwcmd];

        if (!callback) {
                pch_hldev_end_reject(devib, EINVALIDCMD);
                return;
        }

        callback(devib);
}
