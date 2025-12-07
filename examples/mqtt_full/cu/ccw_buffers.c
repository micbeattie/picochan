/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "mqtt_cu.h"
#include "md_ccw.h"

// Called when a WRITE_MESSAGE CCW has received all data available
static void md_ccw_write_message_received(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_required(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        tm->mlen = hd->count;
        pch_hldev_end_ok(devib);
}

// Called to start a WRITE_MESSAGE CCW
void md_ccw_write_message(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        if (!tm->tlen) {
                pch_hldev_end_reject(devib, MD_ERR_NO_TOPIC);
                return;
        }

        // leave topic present but clear any existing message
        tm->mlen = 0;
        pch_hldev_receive_then(devib, tmbuf_message_ptr(tm),
                tmbuf_message_replace_maxlen(tm),
                md_ccw_write_message_received);
}

// Called when a WRITE_MESSAGE CCW has received all data available
static void md_ccw_write_message_append_received(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_required(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        tm->mlen += hd->count;
        pch_hldev_end_ok(devib);
}

// Called to start a WRITE_MESSAGE_APPEND CCW
void md_ccw_write_message_append(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        pch_hldev_receive_then(devib, tmbuf_message_append_ptr(tm),
                tmbuf_message_append_maxlen(tm),
                md_ccw_write_message_append_received);
}

// Called when a WRITE_TOPIC CCW has received all data available
void md_ccw_write_topic_received(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_required(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        tm->tlen = hd->count;
        tm->buf[tm->tlen] = '\0'; // guaranteed room
        pch_hldev_end_ok(devib);
}

// Called to start a WRITE_TOPIC CCW
void md_ccw_write_topic(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        tmbuf_reset(tm);
        pch_hldev_receive_then(devib, tmbuf_topic_ptr(tm),
                tmbuf_topic_maxlen(tm), md_ccw_write_topic_received);
}

// Called when a WRITE_TOPIC_AND_MESSAGE CCW has received all data available
static void md_ccw_write_topic_and_message_received(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_required(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        if (tmbuf_parse(tm, hd->count))
                pch_hldev_end_ok(devib);
        else
                pch_hldev_end_reject(devib, EINVALIDVALUE);
}

// Called to start a WRITE_TOPIC_AND_MESSAGE CCW
void md_ccw_write_topic_and_message(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        tmbuf_reset(tm);
        pch_hldev_receive_then(devib, tm->buf, sizeof(tm->buf),
                md_ccw_write_topic_and_message_received);
}

// Called to start a SET_CURRENT_ID CCW
void md_ccw_set_current_id(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        md->cur = 0;
        pch_hldev_receive_buffer_final(devib, &md->cur, sizeof(md->cur));
}

// Called to start a READ_MESSAGE CCW
void md_ccw_read_message(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        if (!tm->tlen) {
                pch_hldev_end_exception(devib);
                return;
        }

        pch_hldev_send_final(devib, tmbuf_message_ptr(tm), tm->mlen);
}

// Called to start a READ_TOPIC CCW
void md_ccw_read_topic(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        if (!tm->tlen) {
                pch_hldev_end_exception(devib);
                return;
        }

        pch_hldev_send_final(devib, tmbuf_topic_ptr(tm), tm->tlen);
}

// Called to start a READ_TOPIC_AND_MESSAGE CCW
void md_ccw_read_topic_and_message(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        if (!tm->tlen) {
                pch_hldev_end_exception(devib);
                return;
        }

        pch_hldev_send_final(devib, tmbuf_topic_ptr(tm),
                tm->tlen + 1 + tm->mlen);
}

// Called to start any CCW which just needs to verify that a valid
// current tmbuf is set and, if so, append a task for this md to
// the task_list. This includes PUBLISH, SUBSCRIBE, UNSUBSCRIBE.
void md_ccw_start_task_with_current_tmbuf(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        md_task_list_append(devib);
}
