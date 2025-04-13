/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "picochan/devib.h"
#include "picochan/dev_status.h"
#include "cus_trace.h"

pch_devib_callback_t pch_devib_callbacks[NUM_DEVIB_CALLBACKS];

void __time_critical_func(pch_register_devib_callback)(pch_cbindex_t n, pch_devib_callback_t cb) {
        valid_params_if(PCH_CUS, n < NUM_DEVIB_CALLBACKS);
        assert(pch_devib_callbacks[n] == NULL);

	trace_register_callback(PCH_TRC_RT_CUS_REGISTER_CALLBACK, n, cb);

	pch_devib_callbacks[n] = cb;
}

pch_cbindex_t __time_critical_func(pch_register_unused_devib_callback)(pch_devib_callback_t cb) {
	// Simple linear search for unset function will suffice for this
	for (int n = 0; n < NUM_DEVIB_CALLBACKS; n++) {
		if (!pch_devib_callbacks[n]) {
                        trace_register_callback(PCH_TRC_RT_CUS_REGISTER_CALLBACK,
                                n, cb);
			pch_devib_callbacks[n] = cb;
			return (pch_cbindex_t)n;
		}
	}

	panic("no more room in pch_devib_callbacks array");
}

void __time_critical_func(pch_default_devib_callback)(pch_cu_t *cu, pch_devib_t *devib) {
        pch_unit_addr_t ua = pch_get_ua(cu, devib);
        proto_chop_cmd_t cmd = proto_chop_cmd(devib->op);
        pch_dev_sense_t sense;

        switch (cmd) {
        case PROTO_CHOP_START:
                sense = (pch_dev_sense_t){
                        .flags = PCH_DEV_SENSE_COMMAND_REJECT,
                        .code = EINVALIDDEV,
                };
                pch_dev_update_status_error(cu, ua, sense);
                break;

        default:
                sense = (pch_dev_sense_t){
                        .flags = PCH_DEV_SENSE_PROTO_ERROR,
                        .code = devib->op,
                        .asc = devib->payload.p0,
                        .ascq = devib->payload.p1
                };
                pch_dev_update_status_error(cu, ua, sense);
                break;
        }
}
