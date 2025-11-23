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
