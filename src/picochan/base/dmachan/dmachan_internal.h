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
        return spin_lock_blocking(dmachan_mem_peer_spin_lock);
}

static inline void mem_peer_unlock(uint32_t saved_irq) {
        return spin_unlock(dmachan_mem_peer_spin_lock, saved_irq);
}

// trigger_irq sets the control register for dmaid with just the
// minimal bits (Enabled and Quiet) and then writes a 0 to the
// register so that it raises the IRQ from this channel without
// actual doing the DMA copy. The write of zero leaves the
// control register as zero so we rely on the next use of the
// DMA writing the whole control register correctly again.
static inline void trigger_irq(pch_dmaid_t dmaid) {
        dma_channel_config czero = {0}; // zero, *not* default config
        dma_channel_config c = czero;
        channel_config_set_irq_quiet(&c, true);
        channel_config_set_enable(&c, true);
        dma_channel_set_config(dmaid, &c, false);
        dma_channel_set_config(dmaid, &czero, true);
}

#endif
