/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "picochan/hldev.h"

void pch_hldev_dev_range_init(pch_dev_range_t *dr, pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices, pch_devib_callback_t start_devib) {
        pch_dev_range_init(dr, cu, first_ua, num_devices);
        pch_dev_range_register_unused_devib_callback(dr, start_devib);
}

void pch_hldev_config_init(pch_hldev_config_t *hdcfg, pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices, pch_devib_callback_t start_devib) {
        pch_hldev_dev_range_init(&hdcfg->dev_range,
                cu, first_ua, num_devices, start_devib);
}

void pch_hldev_reset(pch_hldev_config_t *hdcfg, pch_hldev_t *hd) {
        hd->callback = hdcfg->start;
        hd->state = PCH_HLDEV_IDLE;
        hd->addr = NULL;
        hd->size = 0;
        hd->count = 0;
}

// do_receive progresses an hldev in RECEIVING state, meaning that it
// has requested to receive data from a Write-type CCW into a sized
// buffer. Unlike a low-level pch_dev_receive() which receives at most
// to the end of the current segment, this function repeatedly calls
// pch_dev_receive() to fill in as much of the requested buffer as
// possible. The first call to pch_dev_receive() is from
// pch_hldev_receive() so by the time we are called, the devib
// contains the information sent by the CSS about the latest receive.
static void do_receive(pch_hldev_config_t *hdcfg, pch_hldev_t *hd, pch_devib_t *devib) {
        uint16_t n = proto_parse_count_payload(devib->payload);
        assert((uint)hd->count + (uint)n <= (uint)hd->size);
        hd->count += n;
        hd->addr += n;
        uint16_t remaining = hd->size - hd->count;
        bool eof = pch_devib_is_stopping(devib)
                || proto_chop_has_end(devib->op);
        if (eof)
                hd->flags |= PCH_HLDEV_FLAG_EOF;

        if (remaining > 0 && !eof) {
                pch_dev_receive(devib, hd->addr, remaining);
                return;
        }

        hd->state = PCH_HLDEV_STARTED;
        hd->callback(hdcfg, devib);
}

void pch_hldev_receive_then(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *dstaddr, uint16_t size, pch_hldev_callback_t callback) {
        pch_hldev_t *hd = pch_hldev_get(hdcfg, devib);
        if (!pch_devib_is_cmd_write(devib)) {
                pch_hldev_end_proto_error(hdcfg, devib,
                        PCH_HLDEV_ERR_RECEIVE_FROM_READ_CCW);
                pch_hldev_reset(hdcfg, hd);
                return;
        }

        if (callback)
                hd->callback = callback;

        hd->addr = dstaddr;
        hd->size = size;
        hd->count = 0;
        hd->state = PCH_HLDEV_RECEIVING;
        pch_dev_receive(devib, dstaddr, size);
}

void pch_hldev_receive(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *dstaddr, uint16_t size) {
        pch_hldev_receive_then(hdcfg, devib, dstaddr, size, NULL);
}

// do_send progresses an hldev in SENDING state, meaning that it
// has requested to send data to a Read-type CCW from a sized buffer.
// Unlike a low-level pch_dev_send() which sends at most to the end of
// the current segment, this function repeatedly calls pch_dev_send()
// to send as much of the requested buffer as possible.
// The first call to pch_dev_send() is from pch_hldev_send() so by the
// time we are called, devib->size contains the exact remaining size
// of the segment.
static void do_send(pch_hldev_config_t *hdcfg, pch_hldev_t *hd, pch_devib_t *devib) {
        void *srcaddr = hd->addr;
        uint16_t n = hd->size - hd->count;
        assert(n > 0);

        if (n > devib->size) {
                n = devib->size;
        } else {
                // this will be the last send
                hd->state = PCH_HLDEV_STARTED;
        }

        hd->addr += n;
        hd->count += n;
        pch_dev_send(devib, srcaddr, n, 0);
}

void pch_hldev_send_then(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *srcaddr, uint16_t size, pch_hldev_callback_t callback) {
        pch_hldev_t *hd = pch_hldev_get(hdcfg, devib);
        if (pch_devib_is_cmd_write(devib)) {
                pch_hldev_end_proto_error(hdcfg, devib,
                        PCH_HLDEV_ERR_SEND_TO_WRITE_CCW);
                pch_hldev_reset(hdcfg, hd);
                return;
        }

        if (callback)
                hd->callback = callback;

        hd->size = size;
        if (size <= devib->size) {
                // enough announced room in segment to send it all
                // here without needing to go into SENDING state
                devib->size -= size; // XXX check this is OK
                hd->count = size;
                pch_dev_send(devib, srcaddr, size, 0);
                return;
        }

        size = devib->size;
        hd->count = size;
        hd->addr = srcaddr + size;
        hd->state = PCH_HLDEV_SENDING;
        pch_dev_send(devib, srcaddr, size, PROTO_CHOP_FLAG_RESPONSE_REQUIRED);
}

void pch_hldev_send(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *srcaddr, uint16_t size) {
        pch_hldev_send_then(hdcfg, devib, srcaddr, size, NULL);
}

void pch_hldev_end(pch_hldev_config_t *hdcfg, pch_devib_t *devib, uint8_t extra_devs, pch_dev_sense_t sense) {
        pch_hldev_t *hd = pch_hldev_get(hdcfg, devib);
        extra_devs |= PCH_DEVS_CHANNEL_END|PCH_DEVS_DEVICE_END;
        if (sense.flags)
                extra_devs |= PCH_DEVS_UNIT_CHECK;

        hd->callback = hdcfg->start;
        hd->state = PCH_HLDEV_IDLE;
        devib->sense = sense;
        pch_dev_update_status(devib, extra_devs);
}

void pch_hldev_call_callback(pch_hldev_config_t *hdcfg, pch_devib_t *devib) {
        if (pch_devib_is_stopping(devib)) {
                if (hdcfg->signal)
                        hdcfg->signal(hdcfg, devib);
                else
                        pch_hldev_end_stopped(hdcfg, devib);
                return;
        }

        pch_hldev_t *hd = pch_hldev_get(hdcfg, devib);
        if (!hd) {
                pch_hldev_end_reject(hdcfg, devib, EINVALIDDEV);
                return;
        }

        switch (hd->state) {
        case PCH_HLDEV_IDLE:
                pch_hldev_callback_t start = hdcfg->start;
                if (!start) {
                        pch_hldev_end_proto_error(hdcfg, devib,
                                PCH_HLDEV_ERR_NO_START_CALLBACK);
                        return;
                }
                hd->callback = hdcfg->start;
                // fallthrough

        case PCH_HLDEV_STARTED:
                hd->callback(hdcfg, devib);
                return;

        case PCH_HLDEV_RECEIVING:
                do_receive(hdcfg, hd, devib);
                return;

        case PCH_HLDEV_SENDING:
                do_send(hdcfg, hd, devib);
                return;
        }

        pch_dev_update_status_error(devib, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_COMMAND_REJECT,
                .code = EINVALIDSTATUS,
                .asc = hd->state
        }));
        pch_hldev_reset(hdcfg, hd);
}
