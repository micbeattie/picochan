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

static inline dmachan_1way_config_t dmachan_1way_config_memchan_make(pch_dmaid_t dmaid, pch_irq_index_t dmairqix) {
        dma_channel_config ctrl = dma_channel_get_default_config(dmaid);
        channel_config_set_transfer_data_size(&ctrl, DMA_SIZE_8);
        channel_config_set_read_increment(&ctrl, true);
        channel_config_set_write_increment(&ctrl, true);
        return ((dmachan_1way_config_t){
                .addr = 0,
                .ctrl = ctrl,
                .dmaid = dmaid,
                .dmairqix = dmairqix
        });
}

static inline dmachan_config_t dmachan_config_memchan_make(pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, pch_irq_index_t dmairqix) {
        return ((dmachan_config_t){
                .tx = dmachan_1way_config_memchan_make(txdmaid, dmairqix),
                .rx = dmachan_1way_config_memchan_make(rxdmaid, dmairqix)
        });
}

static dmachan_config_t claim_dma_channels(uint dmairqix) {
        pch_dmaid_t txdmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        pch_dmaid_t rxdmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        return dmachan_config_memchan_make(txdmaid, rxdmaid, dmairqix);
}

static dmachan_config_t import_dma_channels(uint dmairqix, pch_channel_t *chpeer) {
        assert(pch_channel_is_configured(chpeer));
        // Import rx <-> tx dmaids
        pch_dmaid_t txdmaid = chpeer->rx.link.dmaid;
        pch_dmaid_t rxdmaid = chpeer->tx.link.dmaid;
        return dmachan_config_memchan_make(txdmaid, rxdmaid, dmairqix);
}

static void do_init_memchan(pch_channel_t *ch, dmachan_config_t *dc) {
        dmachan_init_tx_channel(&ch->tx, &dc->tx, &dmachan_mem_tx_channel_ops);
        // Do not enable irq for tx channel link because Pico DMA
        // does not treat the INTSn bits separately. We enable only
        // the rx side for irqs and the rx irq handler propagates
        // notifications to the tx side via the INTFn "forced irq"
        // register which overrides the INTEn enabled bits.

        dmachan_rx_channel_t *rx = &ch->rx;
        dmachan_init_rx_channel(rx, &dc->rx, &dmachan_mem_rx_channel_ops);
        dmachan_set_link_dma_irq_enabled(&rx->link, true);
}

void pch_channel_init_memchan(pch_channel_t *ch, uint8_t id, uint dmairqix, pch_channel_t *chpeer) {
        assert(!pch_channel_is_started(ch));
        assert(!pch_channel_is_configured(ch));

        dmachan_config_t dc;
        if (pch_channel_is_configured(chpeer))
                dc = import_dma_channels(dmairqix, chpeer);
        else
                dc = claim_dma_channels(dmairqix);

        do_init_memchan(ch, &dc);

        dmachan_rx_channel_t *rx = &ch->rx;
        dmachan_tx_channel_t *txpeer = &chpeer->tx;
        txpeer->u.mem.rx_peer = rx;
        rx->u.mem.tx_peer = txpeer;
        pch_channel_configure_id(ch, id);
}
