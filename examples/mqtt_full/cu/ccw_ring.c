/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <hardware/sync.h>
#include "mqtt_cu.h"
#include "md_ccw.h"

// Called to do a SET_FILTER_ID CCW
void md_ccw_set_filter_id(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        md->filt = 0;
        pch_hldev_receive_buffer_final(devib, &md->filt, sizeof(md->filt));
}

// Called to do a WAIT CCW
void md_ccw_wait(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        if (!md_ring_is_started(md)) {
                pch_hldev_end_reject(devib, MD_ERR_RING_NOT_STARTED);
                return;
        }

        assert(md_ring_valid(&md->ring));

        uint32_t status = md_ring_lock();
        uint16_t next = md->ring.next;
        uint16_t cur = md->cur;
        md_ring_unlock(status);

        if (next != cur) {
                pch_hldev_end_ok(devib);
                return;
        }

        // leave channel program running - md_wake() will be called
        // by md_inpub_data_cb() when a new topic/message is written
        // to the ring and that will end the channel program so that
        // the application can issue a READ to fetch the new message.
}

// Called to do a START_RING CCW
void md_ccw_start_ring(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        if (md_ring_is_started(md)) {
                pch_hldev_end_reject(devib, MD_ERR_RING_STARTED);
                return;
        }

        if (!md_ring_valid(&md->ring)) {
                pch_hldev_end_reject(devib, MD_ERR_RING_INVALID);
                return;
        }

        md->ring.full = MD_RING_NOT_FULL;
        md_set_ring_is_started(md, true);
        pch_hldev_end_ok(devib);
}

// Called to do a STOP_RING CCW
void md_ccw_stop_ring(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        if (!md_ring_is_started(md)) {
                pch_hldev_end_reject(devib, MD_ERR_RING_NOT_STARTED);
                return;
        }

        assert(md_ring_valid(&md->ring));
        md_set_ring_is_started(md, false);
        pch_hldev_end_ok(devib);
}

// Called when a SET_RING CCW has received all data available
static void md_ccw_set_ring_received(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        uint8_t err = 0;

        md_ring_t *mr = &md->ring;
        if (md->hldev.count != sizeof(*mr))
                err = EBUFFERTOOSHORT;
        else if (!md_ring_valid(mr))
                err = MD_ERR_RING_INVALID;

        if (err) {
                memset(&md->ring, 0, sizeof(md->ring));
                pch_hldev_end_reject(devib, err);
                return;
        }

        pch_hldev_end_ok(devib);
}

// Called to do a SET_RING CCW
void md_ccw_set_ring(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        if (md_ring_is_started(md)) {
                pch_hldev_end_reject(devib, MD_ERR_RING_STARTED);
                return;
        }

        memset(&md->ring, 0, sizeof(md->ring));
        pch_hldev_receive_then(devib, &md->ring, sizeof(md->ring),
                md_ccw_set_ring_received);
}

// Called to do a GET_RING CCW
void md_ccw_get_ring(pch_devib_t *devib) {
        mqtt_dev_t *md = get_mqtt_dev(devib);
        pch_hldev_send_final(devib, &md->ring, sizeof(md->ring));
}

// Called to do an ACK CCW
void md_ccw_ack(pch_devib_t *devib) {
        tmbuf_t *tm = get_current_tmbuf_or_reject(devib);
        if (!tm)
                return;

        mqtt_dev_t *md = get_mqtt_dev(devib);
        md_ring_t *mr = &md->ring;
        assert(md_ring_valid(mr));

        if (!md_ring_is_started(md)) {
                pch_hldev_end_reject(devib, MD_ERR_RING_NOT_STARTED);
                return;
        }

        uint8_t extra_devs = 0;

        uint32_t status = md_ring_lock();
        uint16_t cur = md->cur;
        if (mr->full == cur) {
                extra_devs = PCH_DEVS_UNIT_EXCEPTION;
                mr->full = MD_RING_NOT_FULL;
        }
        md->cur = md_ring_increment(mr, cur);
        md_ring_unlock(status);

        tmbuf_reset(tm);
        pch_hldev_end(devib, extra_devs, PCH_DEV_SENSE_NONE);
}
