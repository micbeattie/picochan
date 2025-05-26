/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_TXSM_STATE_H
#define _PCH_API_TXSM_STATE_H

typedef enum __attribute__((packed)) pch_txsm_state {
        PCH_TXSM_IDLE = 0,
        PCH_TXSM_PENDING,
        PCH_TXSM_SENDING
} pch_txsm_state_t;

typedef enum __attribute__((packed)) pch_txsm_run_result {
        PCH_TXSM_NOOP = 0,
        PCH_TXSM_ACTED,
        PCH_TXSM_FINISHED
} pch_txsm_run_result_t;

#endif
