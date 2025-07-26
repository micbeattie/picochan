/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CUS_CALLBACK_H
#define _PCH_CUS_CALLBACK_H

#include "picochan/cu.h"
#include "picochan/devib.h"
#include "picochan/dev_status.h"
#include "cus_trace.h"

static inline void pch_call_devib_callback(pch_cbindex_t cbindex, pch_devib_t *devib) {
        assert(pch_cbindex_is_callable(cbindex));

	if (cbindex == PCH_DEVIB_CALLBACK_NOOP)
		return;

	pch_devib_callback_t cb = pch_devib_callbacks[cbindex];
	cb(devib);
}

static inline void callback_devib(pch_devib_t *devib) {
        pch_cbindex_t cbindex = devib->cbindex;

        trace_call_callback(PCH_TRC_RT_CUS_CALL_CALLBACK,
                devib, cbindex);
        pch_call_devib_callback(cbindex, devib);
}

#endif
