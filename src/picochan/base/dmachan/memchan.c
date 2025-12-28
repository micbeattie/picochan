/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"

// mem_peer_spin_lock must be initialised with pch_memchan_init
spin_lock_t *dmachan_mem_peer_spin_lock;

void dmachan_panic_unless_memchan_initialised() {
        if (!dmachan_mem_peer_spin_lock)
                panic("pch_memchan_init not called");
}

// pch_memchan_init must be called to initialise mem_peer_spin_lock
// before configuring any memchan CU. If it is not initialised then
// any attempt to configure a memcu from either CSS or CUS side
// will panic at runtime to avoid any mysterious race conditions.
void pch_memchan_init(void) {
        if (dmachan_mem_peer_spin_lock)
                panic("dmachan_mem_peer_spin_lock already initialised");

        int n = spin_lock_claim_unused(true);
        dmachan_mem_peer_spin_lock = spin_lock_init(n);
}

void dmachan_init_mem_channel(pch_channel_t *ch, dmachan_config_t *dc, pch_channel_t *chpeer) {
        assert(!pch_channel_is_started(ch));
        dmachan_init_tx_channel(&ch->tx, &dc->tx,
                &dmachan_mem_tx_channel_ops);
        // Do not enable irq for tx channel link because Pico DMA
        // does not treat the INTSn bits separately. We enable only
        // the rx side for irqs and the rx irq handler propagates
        // notifications to the tx side via the INTFn "forced irq"
        // register which overrides the INTEn enabled bits.

        dmachan_rx_channel_t *rx = &ch->rx;
        dmachan_init_rx_channel(rx, &dc->rx,
                &dmachan_mem_rx_channel_ops);
        dmachan_set_link_irq_enabled(&rx->link, true);
        dmachan_tx_channel_t *txpeer = &chpeer->tx;
        txpeer->mem_rx_peer = rx;
        rx->mem_tx_peer = txpeer;
        pch_channel_set_configured(ch, true);
}
