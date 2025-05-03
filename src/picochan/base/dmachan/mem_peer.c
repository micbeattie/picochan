/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "picochan/dmachan.h"
#include "mem_peer.h"

// mem_peer_spin_lock must be initialised with init_mem_peer_lock
spin_lock_t *mem_peer_spin_lock;

void init_mem_peer_lock(void) {
        int n = spin_lock_claim_unused(true);
        mem_peer_spin_lock = spin_lock_init(n);
}

void dmachan_mem_peer_connect(dmachan_tx_channel_t *tx, dmachan_rx_channel_t *rx) {
        pch_dmaid_t dmaid = tx->dmaid;
        valid_params_if(PCH_DMACHAN, dmaid == rx->dmaid);

        tx->mem_rx_peer = rx;
        rx->mem_tx_peer = tx;

        dma_channel_config c = dma_channel_get_default_config(dmaid);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, true);
        rx->ctrl = c;
        dma_channel_set_config(dmaid, &c, false);
}
