/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_DMACHAN_MEMCHAN_INTERNAL_H
#define _PCH_DMACHAN_MEMCHAN_INTERNAL_H

#include "dmachan_internal.h"
#include "dmachan_trace.h"

// dmachan_mem_peer_spin_lock protects against test/update of
// tx_channel.mem_src_state and rx_channel.mem_dst_state both
// from interrupts and cross-core. It must be initialised before
// use with pch_memchan_init().
extern spin_lock_t *dmachan_mem_peer_spin_lock;

static inline uint32_t mem_peer_lock(void) {
        return spin_lock_blocking(dmachan_mem_peer_spin_lock);
}

static inline void mem_peer_unlock(uint32_t saved_irq) {
        spin_unlock(dmachan_mem_peer_spin_lock, saved_irq);
}

#ifndef PCH_DMACHAN_MEMCHAN_DEBUG_ENABLED
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
#define PCH_DMACHAN_MEMCHAN_DEBUG_ENABLED true
#else
#define PCH_DMACHAN_MEMCHAN_DEBUG_ENABLED false
#endif
#endif

#define PCH_DMACHAN_LINK_MEMCHAN_DEBUG_TRACE(rt, l, data) \
        PCH_TRC_WRITE(l->bs, PCH_DMACHAN_MEMCHAN_DEBUG_ENABLED && l->bs, (rt), (data))

static inline void trace_dmachan_segment_memstate(pch_trc_record_type_t rt, dmachan_link_t *l, uint32_t addr, uint32_t count, uint8_t state) {
        PCH_DMACHAN_LINK_TRACE(rt, l,
                ((struct pch_trdata_dmachan_segment_memstate){
                        .addr = addr,
                        .count = count,
                        .dmaid = l->dmaid,
                        .state = state
                }));
}

static inline void trace_dmachan_cmd(pch_trc_record_type_t rt, dmachan_link_t *l) {
        PCH_DMACHAN_LINK_MEMCHAN_DEBUG_TRACE(rt, l,
                ((struct pch_trdata_dmachan_cmd){
                        .cmd = l->cmd.raw,
                        .seqnum = dmachan_link_seqnum(l),
                        .dmaid = l->dmaid
                }));
}

#endif
