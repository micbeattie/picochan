/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_TXSM_PENDING_XFER_H
#define _PCH_TXSM_PENDING_XFER_H

// PICO_CONFIG: PARAM_ASSERTIONS_ENABLED_PCH_TXSM, Enable/disable assertions in the pch_txsm module, type=bool, default=0, group=pch_txsm
#ifndef PARAM_ASSERTIONS_ENABLED_PCH_TXSM
#define PARAM_ASSERTIONS_ENABLED_PCH_TXSM 0
#endif

#include <stdint.h>
#include "picochan/dmachan.h"
#include "picochan/txsm_state.h"

// txsm provides a state machine that manages using a
// dmachan_tx_channel to transmit a data buffer, driven by
// tx completion handler calls.
//
// pch_txsm_t represents a pending data transfer.
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |               |     flags     |          count                |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                             addr                              |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

typedef struct pch_txsm {
        pch_txsm_state_t        state;
        uint16_t                count;
        uint32_t                addr;
} pch_txsm_t;

// pch_txsm_busy returns whether px is non-Idle (i.e. it returns true
// if and only if px is in state Pending or Sending).
static inline bool pch_txsm_busy(pch_txsm_t *px) {
        return px->state != PCH_TXSM_IDLE;
}

// Reset resets the state to Idle but does not change any
// owner, addr or count set by SetPending
static inline void pch_txsm_reset(pch_txsm_t *px) {
        px->state = PCH_TXSM_IDLE;
}

// pch_txsm_set_pending stashes (addr, count) in px and moves its
// state from Idle to Pending. It panics if px is Busy.
static inline void pch_txsm_set_pending(pch_txsm_t *px, uint32_t addr, uint16_t count) {
        valid_params_if(PCH_TXSM, px->state == PCH_TXSM_IDLE);

        px->state = PCH_TXSM_PENDING;
        px->addr = addr;
        px->count = count;
}

typedef enum __packed pch_txsm_run_result {
        PCH_TXSM_NOOP = 0,
        PCH_TXSM_ACTED,
        PCH_TXSM_FINISHED
} pch_txsm_run_result_t;

enum pch_txsm_run_result pch_txsm_run(pch_txsm_t *px, dmachan_tx_channel_t *txch);

#endif
