/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "picochan/hldev.h"
#include "hldev_trace.h"

void pch_hldev_reset(pch_hldev_config_t *hdcfg, pch_hldev_t *hd) {
        hd->callback = hdcfg->start;
        hd->addr = NULL;
        hd->size = 0;
        hd->count = 0;
        hd->state = PCH_HLDEV_IDLE;
        hd->flags = 0;
        hd->ccwcmd = 0;
}

void pch_hldev_end_ok(pch_devib_t *devib) {
        pch_hldev_end_ok_sense(devib, PCH_DEV_SENSE_NONE);
}

// do_receive progresses an hldev in RECEIVING state, meaning that it
// has requested to receive data from a Write-type CCW into a sized
// buffer. Unlike a low-level pch_dev_receive() which receives at most
// to the end of the current segment, this function repeatedly calls
// pch_dev_receive() to fill in as much of the requested buffer as
// possible. The first call to pch_dev_receive() is from
// pch_hldev_receive() so by the time we are called, the devib
// contains the information sent by the CSS about the latest receive.
static void do_receive(pch_hldev_t *hd, pch_devib_t *devib) {
        assert(pch_devib_is_cmd_write(devib));
        uint16_t n = proto_parse_count_payload(devib->payload);
        assert((uint)hd->count + (uint)n <= (uint)hd->size);
        hd->count += n;
        hd->addr += n;
        uint16_t remaining = hd->size - hd->count;
        bool eof = pch_devib_is_stopping(devib)
                || proto_chop_has_end(devib->op);
        if (eof)
                hd->flags |= PCH_HLDEV_FLAG_EOF;

        uint16_t next_count = 0;
        if (remaining > 0 && !eof)
                next_count = remaining;

        trace_hldev_counts(PCH_TRC_RT_HLDEV_RECEIVING,
                devib, n, next_count);
        if (next_count) {
                pch_dev_receive(devib, hd->addr, next_count);
                return;
        }

        hd->state = PCH_HLDEV_STARTED;
        hd->callback(devib);
}

void pch_hldev_receive_then(pch_devib_t *devib, void *dstaddr, uint16_t size, pch_devib_callback_t callback) {
        pch_hldev_t *hd = pch_hldev_get(devib);
        assert(pch_hldev_is_started(hd));
        assert(pch_devib_is_cmd_write(devib));

        if (callback)
                hd->callback = callback;

        hd->addr = dstaddr;
        hd->size = size;
        hd->count = 0;
        hd->state = PCH_HLDEV_RECEIVING;

        if (callback) {
                trace_hldev_data_then(PCH_TRC_RT_HLDEV_RECEIVE_THEN,
                        devib, dstaddr, size, callback);
        } else {
                trace_hldev_data(PCH_TRC_RT_HLDEV_RECEIVE,
                        devib, dstaddr, size);
        }

        pch_dev_receive(devib, dstaddr, size);
}

void pch_hldev_receive(pch_devib_t *devib, void *dstaddr, uint16_t size) {
        pch_hldev_receive_then(devib, dstaddr, size, NULL);
}

void pch_hldev_terminate_string(pch_devib_t *devib) {
        pch_hldev_t *hd = pch_hldev_get(devib);
        *(char*)hd->addr = '\0';
        hd->addr++;
        hd->count++;
}

void pch_hldev_terminate_string_end_ok(pch_devib_t *devib) {
        pch_hldev_terminate_string(devib);
        pch_hldev_end_ok(devib);
}

void pch_hldev_receive_string_final(pch_devib_t *devib, void *dstaddr, uint16_t len) {
        pch_hldev_receive_then(devib, dstaddr, len,
                pch_hldev_terminate_string_end_ok);
}

void pch_hldev_receive_buffer_final(pch_devib_t *devib, void *dstaddr, uint16_t size) {
        pch_hldev_receive_then(devib, dstaddr, size, pch_hldev_end_ok);
}

// do_send progresses an hldev in SENDING or SENDING_FINAL state,
// meaning that it has requested to send data to a Read-type CCW from
// a sized buffer. Unlike a low-level pch_dev_send() which sends at
// most to the end of the current segment, this function repeatedly
// calls pch_dev_send() to send as much of the requested buffer as
// possible. The first call to pch_dev_send() is from pch_hldev_send()
// so by the time we are called, devib->size contains the exact
// remaining size of the segment. If we send the last chunk of data
// this time then for SENDING state, we return to STARTED state or
// else for SENDING_FINAL, we include the PROTO_CHOP_FLAG_END flag
// with the pch_dev_send() so that the CSS treats it as an implicit
// "normal" end (DEVICE_END|CHANNEL_END with no sense) and we can go
// straight to IDLE state.
static void do_send(pch_hldev_t *hd, pch_devib_t *devib) {
        assert(!pch_devib_is_cmd_write(devib));
        void *srcaddr = hd->addr;
        uint16_t n = hd->size - hd->count;
        assert(n > 0);

        bool final = pch_hldev_is_sending_final(hd);
        bool end = false;
        if (n > devib->size) {
                n = devib->size;
        } else if (final) {
                end = true;
                hd->state = PCH_HLDEV_ENDING;
        } else {
                hd->state = PCH_HLDEV_STARTED;
        }

        trace_hldev_counts(PCH_TRC_RT_HLDEV_SENDING, devib, n,
                devib->size);

        proto_chop_flags_t flags = 0;
        if (end) {
                flags = PROTO_CHOP_FLAG_END;
        } else {
                hd->addr += n;
                hd->count += n;
        }

        pch_dev_send(devib, srcaddr, n, flags);
}

static void start_send(pch_devib_t *devib, void *srcaddr, uint16_t size, pch_devib_callback_t callback, bool final) {
        pch_hldev_t *hd = pch_hldev_get(devib);
        assert(pch_hldev_is_started(hd));
        assert(!pch_devib_is_cmd_write(devib));
        assert(size);

        if (callback)
                hd->callback = callback;

        proto_chop_flags_t flags = 0;
        if (final)
                flags = PROTO_CHOP_FLAG_END;

        hd->size = size;
        if (size <= devib->size) {
                // enough announced room in segment to send it all
                // here without needing to go into SENDING state
                if (final) {
                        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
                        pch_hldev_reset(hdcfg, hd); // back to IDLE
                } else {
                        devib->size -= size; // XXX check this is OK
                        hd->count = size;
                }
        } else {
                if (final) {
                        hd->state = PCH_HLDEV_SENDING_FINAL;
                } else {
                        flags |= PROTO_CHOP_FLAG_RESPONSE_REQUIRED;
                        size = devib->size;
                        hd->count = size;
                        hd->addr = srcaddr + size;
                        hd->state = PCH_HLDEV_SENDING;
                }
        }

        if (callback) {
                pch_trc_record_type_t rt = final ?
                        PCH_TRC_RT_HLDEV_SEND_FINAL_THEN : PCH_TRC_RT_HLDEV_SEND_THEN;
                trace_hldev_data_then(rt, devib, srcaddr, size, callback);
        } else {
                pch_trc_record_type_t rt = final ?
                        PCH_TRC_RT_HLDEV_SEND_FINAL : PCH_TRC_RT_HLDEV_SEND;
                trace_hldev_data(rt, devib, srcaddr, size);
        }

        int rc = pch_dev_send(devib, srcaddr, size, flags);
        assert(rc >= 0);
}

void pch_hldev_send_then(pch_devib_t *devib, void *srcaddr, uint16_t size, pch_devib_callback_t callback) {
        start_send(devib, srcaddr, size, callback, false);
}

void pch_hldev_send_final(pch_devib_t *devib, void *srcaddr, uint16_t size) {
        start_send(devib, srcaddr, size, NULL, true);
}

void pch_hldev_send(pch_devib_t *devib, void *srcaddr, uint16_t size) {
        pch_hldev_send_then(devib, srcaddr, size, NULL);
}

void pch_hldev_end(pch_devib_t *devib, uint8_t extra_devs, pch_dev_sense_t sense) {
        pch_hldev_t *hd = pch_hldev_get(devib);
        assert(pch_hldev_is_started(hd));
        assert(!pch_hldev_is_idle(hd));
        extra_devs |= PCH_DEVS_CHANNEL_END|PCH_DEVS_DEVICE_END;
        if (sense.flags)
                extra_devs |= PCH_DEVS_UNIT_CHECK;

        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        hd->callback = hdcfg->start;
        hd->state = PCH_HLDEV_ENDING;
        devib->sense = sense;
        trace_hldev_end(devib, sense, extra_devs);
        pch_dev_update_status(devib, extra_devs);
}

static void hldev_devib_callback(pch_devib_t *devib) {
        pch_hldev_config_t *hdcfg = pch_devib_callback_context(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        if (!hd) {
                pch_hldev_end_reject(devib, EINVALIDDEV);
                return;
        }

        trace_hldev_byte(PCH_TRC_RT_HLDEV_DEVIB_CALLBACK, devib,
                hd->state);

        if (pch_devib_is_stopping(devib)) {
                if (hdcfg->signal)
                        hdcfg->signal(devib);
                else
                        pch_hldev_end_stopped(devib);
                return;
        }

        switch (hd->state) {
        case PCH_HLDEV_ENDING:
                if (!pch_devib_is_started(devib)) {
                        pch_hldev_reset(hdcfg, hd); // back to IDLE
                        return;
                }
                // fallthrough

        case PCH_HLDEV_IDLE:
                assert(proto_chop_cmd(devib->op) == PROTO_CHOP_START);
                assert(hdcfg->start);
                trace_hldev_start(devib);
                hd->ccwcmd = devib->payload.p0;
                hd->callback = hdcfg->start;
                // fallthrough

        case PCH_HLDEV_STARTED:
                assert(pch_devib_is_started(devib));
                hd->state = PCH_HLDEV_STARTED;
                hd->callback(devib);
                return;

        case PCH_HLDEV_RECEIVING:
                do_receive(hd, devib);
                return;

        case PCH_HLDEV_SENDING:
        case PCH_HLDEV_SENDING_FINAL:
                do_send(hd, devib);
                return;
        }

        pch_dev_update_status_error(devib, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_COMMAND_REJECT,
                .code = EINVALIDSTATUS,
                .asc = hd->state
        }));
        pch_hldev_reset(hdcfg, hd);
}

void pch_hldev_config_init(pch_hldev_config_t *hdcfg, pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices) {
        assert(num_devices > 0);
        pch_dev_range_t *dr = &hdcfg->dev_range;

        pch_dev_range_init(dr, cu, first_ua, num_devices);
        pch_dev_range_register_unused_devib_callback(dr,
                hldev_devib_callback, hdcfg);
        trace_hldev_config_init(hdcfg);
}
