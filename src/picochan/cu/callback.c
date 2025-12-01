/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "picochan/devib.h"
#include "picochan/dev_status.h"
#include "cus_trace.h"
#include "cu_internal.h"

pch_devib_callback_info_t pch_devib_callbacks[NUM_DEVIB_CALLBACKS];

void __time_critical_func(pch_register_devib_callback)(pch_cbindex_t n, pch_devib_callback_t cbfunc, void *cbctx) {
        if (n >= NUM_DEVIB_CALLBACKS)
                panic("cbindex >= NUM_DEVIB_CALLBACKS");

        if (!cbfunc)
                panic("cbfunc NULL");

        pch_devib_callback_info_t *cb = &pch_devib_callbacks[n];
        if (cb->func)
                panic("cbindex already registered");

        trace_register_callback(PCH_TRC_RT_CUS_REGISTER_CALLBACK, n,
                cbfunc, cbctx);

        cb->func = cbfunc;
        cb->context = cbctx;
}

pch_cbindex_t pch_register_unused_devib_callback(pch_devib_callback_t cbfunc, void *cbctx) {
	// Simple linear search for unset function will suffice for this
	for (uint n = 0; n < NUM_DEVIB_CALLBACKS; n++) {
		if (!pch_cbindex_is_registered(n)) {
                        pch_register_devib_callback(n, cbfunc, cbctx);
			return (pch_cbindex_t)n;
		}
	}

	panic("NUM_DEVIB_CALLBACKS already registered");
}

void __time_critical_func(pch_default_devib_callback)(pch_devib_t *devib) {
        proto_chop_cmd_t cmd = proto_chop_cmd(devib->op);
        pch_dev_sense_t sense;

        switch (cmd) {
        case PROTO_CHOP_START:
                sense = (pch_dev_sense_t){
                        .flags = PCH_DEV_SENSE_COMMAND_REJECT,
                        .code = EINVALIDDEV
                };
                pch_dev_update_status_error(devib, sense);
                break;

        case PROTO_CHOP_HALT:
                sense = (pch_dev_sense_t){
                        .flags = PCH_DEV_SENSE_CANCEL
                };
                pch_dev_update_status_error(devib, sense);
                break;

        default:
                assert(0);
                pch_dev_update_status_proto_error(devib);
                break;
        }
}

static void __time_critical_func(handle_reserved_ccw_read_sense)(pch_devib_t *devib, uint16_t count) {
        if (count > sizeof(devib->sense))
                count = sizeof(devib->sense);

        int rc = pch_dev_send_final(devib, &devib->sense, count);
        assert(rc >= 0);
        (void)rc;
}

static void __time_critical_func(handle_reserved_ccw_read)(pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        switch (ccwcmd) {
        case PCH_CCW_CMD_SENSE:
                handle_reserved_ccw_read_sense(devib, count);
                break;

        default:
                pch_dev_sense_t sense = {
                        .flags = PCH_DEV_SENSE_COMMAND_REJECT
                };
                pch_dev_update_status_error(devib, sense);
                break;
        }
}

static void  __time_critical_func(pch_devib_handle_pending_callback)(pch_devib_t *devib) {
        if (pch_devib_is_start_pending(devib)) {
                pch_devib_set_start_pending(devib, false);
                uint8_t ccwcmd = devib->payload.p0;
                if (pch_is_ccw_cmd_read(ccwcmd)
                        && ccwcmd >= PCH_CCW_CMD_FIRST_RESERVED) {
                        handle_reserved_ccw_read(devib, ccwcmd,
                                devib->size);
                        return;
                }

                pch_devib_set_started(devib, true);
        }
                        
        trace_call_callback(PCH_TRC_RT_CUS_CALL_CALLBACK, devib, 0);
        pch_devib_set_callback_pending(devib, false);
        pch_devib_call_callback(devib);
}

static inline bool try_send_pending_tx_command(pch_cu_t *cu) {
        pch_devib_t *devib = pch_cu_head_devib(cu, &cu->tx_list);
        if (!devib)
                return false;

        if (pch_devib_is_tx_busy(devib))
                return false;

        pch_cu_send_pending_tx_command(cu, devib);
        return true;
}

static inline bool try_handling_pending_callback(pch_cu_t *cu) {
        pch_devib_t *devib = pch_cu_pop_devib(cu, &cu->cb_list);
        if (!devib)
                return false;

        pch_devib_handle_pending_callback(devib);
        return true;
}

void __time_critical_func(pch_cus_async_worker_callback)(async_context_t *context, async_when_pending_worker_t *worker) {
        pch_cu_t *cu = worker->user_data;
        dmachan_link_t *rxl = &cu->rx_channel.link;
        dmachan_link_t *txl = &cu->tx_channel.link;
        bool tx_progress;
        bool cb_progress;

        do {
                if (txl->complete) {
                        txl->complete = false;
                        pch_cus_handle_tx_complete(cu);
                }
                
                if (rxl->complete) {
                        rxl->complete = false;
                        pch_cus_handle_rx_complete(cu);
                }

                tx_progress = try_send_pending_tx_command(cu);
                cb_progress = try_handling_pending_callback(cu);

        } while (txl->complete || rxl->complete || tx_progress || cb_progress);
}
