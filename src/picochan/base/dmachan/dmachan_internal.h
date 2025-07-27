/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_DMACHAN_DMACHAN_INTERNAL_H
#define _PCH_DMACHAN_DMACHAN_INTERNAL_H

#include "hardware/sync.h"
#include "dmachan_trace.h"

// dmachan_mem_peer_spin_lock protects against test/update of
// tx_channel.mem_src_state and rx_channel.mem_dst_state both
// from interrupts and cross-core. It must be initialised before
// use with pch_memchan_init().
extern spin_lock_t *dmachan_mem_peer_spin_lock;

static inline uint32_t mem_peer_lock(void) {
#if PCH_CONFIG_ENABLE_MEMCHAN
        return spin_lock_blocking(dmachan_mem_peer_spin_lock);
#else
        return 0;
#endif
}

static inline void mem_peer_unlock(uint32_t saved_irq) {
#if PCH_CONFIG_ENABLE_MEMCHAN
        spin_unlock(dmachan_mem_peer_spin_lock, saved_irq);
#else
        (void)saved_irq;
#endif
}

#endif
