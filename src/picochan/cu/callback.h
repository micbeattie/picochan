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

// These CB_FROM numbers are only used for writing to
// PCH_TRC_RT_CUS_CALL_CALLBACK trace records to help
// troubleshooting
#define CB_FROM_RX_CHOP_ROOM            1
#define CB_FROM_RX_CHOP_HALT            2
#define CB_FROM_RX_CHOP_START_READ      3
#define CB_FROM_RX_CHOP_START_WRITE     4
#define CB_FROM_RX_DATA_COMPLETE        5
#define CB_FROM_TXSM_FINISHED           6
#define CB_FROM_TXSM_NOOP               7

static inline void callback_devib(pch_devib_t *devib, uint8_t from) {
        pch_cbindex_t cbindex = devib->cbindex;

        trace_call_callback(PCH_TRC_RT_CUS_CALL_CALLBACK,
                devib, cbindex, from);
        pch_devib_call_callback(cbindex, devib);
}

#endif
