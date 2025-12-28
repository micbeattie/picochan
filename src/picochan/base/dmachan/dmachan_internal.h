/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_DMACHAN_DMACHAN_INTERNAL_H
#define _PCH_DMACHAN_DMACHAN_INTERNAL_H

#include "hardware/sync.h"
#include "picochan/dmachan.h"
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

void dmachan_handle_rx_resetting(dmachan_rx_channel_t *rx);

void dmachan_init_tx_channel(dmachan_tx_channel_t *tx, dmachan_1way_config_t *d1c, const dmachan_tx_channel_ops_t *ops);
void dmachan_init_rx_channel(dmachan_rx_channel_t *rx, dmachan_1way_config_t *d1c, const dmachan_rx_channel_ops_t *ops);

#endif
