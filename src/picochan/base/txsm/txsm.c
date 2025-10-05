/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "txsm.h"

// pch_txsm_run runs the pch_txsm state machine and is intended to be
// invoked whenever txch has just completed a tx. It progresses
// through states Idle -> Pending -> Sending -> Idle as follows:
//
// (1) if in state Idle, it does nothing
//
// (2) if in state Pending, it changes state Pending -> Sending
//   and configures and starts the txch DMA engine to transmit data
//   (addr, count) down the channel, as set by SetPending
//
// (3) if in state Sending, it changes state Sending -> Idle
//
// The return values are true when:
//
//   acted: case (2)
//
//   finished: case (3)
pch_txsm_run_result_t __time_critical_func(pch_txsm_run)(pch_txsm_t *px, dmachan_tx_channel_t *txch) {
        switch (px->state) {
        case PCH_TXSM_SENDING:
		px->state = PCH_TXSM_IDLE;
		return PCH_TXSM_FINISHED; // Sending -> Idle, finished

        case PCH_TXSM_PENDING:
                px->state = PCH_TXSM_SENDING;
                dmachan_start_src_data(txch, px->addr, (uint32_t)px->count);
		return PCH_TXSM_ACTED;

        default:
                assert(px->state == PCH_TXSM_IDLE);
                return PCH_TXSM_NOOP;
	}

        // NOTREACHED
}
