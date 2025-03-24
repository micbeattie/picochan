/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_DMACHAN_MEM_PEER_H
#define _PCH_DMACHAN_MEM_PEER_H

#include "hardware/sync.h"

// mem_peer_spin_lock protects against test/update of
// tx_channel.mem_src_state and rx_channel.mem_dst_state both
// from interrupts and cross-core. It must be initialised before
// use with init_mem_peer_spin_lock().
extern spin_lock_t *mem_peer_spin_lock;

static inline uint32_t mem_peer_lock(void) {
    return spin_lock_blocking(mem_peer_spin_lock);
}

static inline void mem_peer_unlock(uint32_t saved_irq) {
    return spin_unlock(mem_peer_spin_lock, saved_irq);
}

#endif
