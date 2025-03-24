/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "callback.h"
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
        assert(proto_chop_cmd(devib->op) == PROTO_CHOP_START);

	uint8_t devs = PCH_DEVS_CHANNEL_END | PCH_DEVS_DEVICE_END
                | PCH_DEVS_UNIT_CHECK;
	pch_devib_prepare_update_status(devib, devs, 0, 0);
        pch_unit_addr_t ua = pch_get_ua(cu, devib);
	pch_devib_send_or_queue_command(cu, ua);
}
