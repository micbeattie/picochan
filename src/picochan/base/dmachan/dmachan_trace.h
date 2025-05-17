/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_DMACHAN_DMACHAN_TRACE_H
#define _PCH_DMACHAN_DMACHAN_TRACE_H

#include "trc/trace.h"

// Separate macros with currently the same replacement text but we
// may want to add some stronger type checks at some point
#define PCH_DMACHAN_TX_TRACE(rt, tx, data) \
        PCH_TRC_WRITE(tx->bs, tx->bs, (rt), (data))

#define PCH_DMACHAN_RX_TRACE(rt, rx, data) \
        PCH_TRC_WRITE(rx->bs, rx->bs, (rt), (data))

struct trdata_dmachan_memstate {
        pch_dmaid_t     dmaid;
        uint8_t         state;
};

struct trdata_dmachan_segment {
        uint32_t        addr;
        uint32_t        count;
        pch_dmaid_t     dmaid;
};

struct trdata_dmachan_segment_memstate {
        uint32_t        addr;
        uint32_t        count;
        pch_dmaid_t     dmaid;
        uint8_t         state;
};

static inline void trace_dmachan_tx_segment(pch_trc_record_type_t rt, dmachan_tx_channel_t *tx, uint32_t addr, uint32_t count) {
        PCH_DMACHAN_TX_TRACE(rt, tx, ((struct trdata_dmachan_segment){
                .addr = addr,
                .count = count,
                .dmaid = tx->dmaid
        }));
}

static inline void trace_dmachan_tx_memstate(pch_trc_record_type_t rt, dmachan_tx_channel_t *tx, uint8_t state) {
        PCH_DMACHAN_TX_TRACE(rt, tx,
                ((struct trdata_dmachan_memstate){
                        .dmaid = tx->dmaid,
                        .state = state
                }));
}

static inline void trace_dmachan_tx_segment_memstate(pch_trc_record_type_t rt, dmachan_tx_channel_t *tx, uint32_t addr, uint32_t count, uint8_t state) {
        PCH_DMACHAN_TX_TRACE(rt, tx,
                ((struct trdata_dmachan_segment_memstate){
                        .addr = addr,
                        .count = count,
                        .dmaid = tx->dmaid,
                        .state = state
                }));
}

static inline void trace_dmachan_rx_segment(pch_trc_record_type_t rt, dmachan_rx_channel_t *rx, uint32_t addr, uint32_t count) {
        PCH_DMACHAN_RX_TRACE(rt, rx, ((struct trdata_dmachan_segment){
                .addr = addr,
                .count = count,
                .dmaid = rx->dmaid
        }));
}

static inline void trace_dmachan_rx_memstate(pch_trc_record_type_t rt, dmachan_rx_channel_t *rx, uint8_t state) {
        PCH_DMACHAN_RX_TRACE(rt, rx,
                ((struct trdata_dmachan_memstate){
                        .dmaid = rx->dmaid,
                        .state = state
                }));
}

static inline void trace_dmachan_rx_segment_memstate(pch_trc_record_type_t rt, dmachan_rx_channel_t *rx, uint32_t addr, uint32_t count, uint8_t state) {
        PCH_DMACHAN_RX_TRACE(rt, rx,
                ((struct trdata_dmachan_segment_memstate){
                        .addr = addr,
                        .count = count,
                        .dmaid = rx->dmaid,
                        .state = state
                }));
}

#endif
