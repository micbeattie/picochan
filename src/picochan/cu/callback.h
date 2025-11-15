/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CUS_CALLBACK_H
#define _PCH_CUS_CALLBACK_H

#include "picochan/cu.h"
#include "picochan/devib.h"
#include "picochan/dev_status.h"
#include "cus_trace.h"

static inline void callback_devib(pch_devib_t *devib) {
        pch_cbindex_t cbindex = devib->cbindex;

        trace_call_callback(PCH_TRC_RT_CUS_CALL_CALLBACK,
                devib, cbindex);
        pch_devib_call_callback(cbindex, devib);
}

#endif
