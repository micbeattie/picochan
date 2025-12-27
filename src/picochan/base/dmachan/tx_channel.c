/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "picochan/dmachan.h"
#include "dmachan_internal.h"
#include "dmachan_trace.h"

void dmachan_init_tx_channel(dmachan_tx_channel_t *tx, dmachan_1way_config_t *d1c) {
        pch_dmaid_t dmaid = d1c->dmaid;
        uint32_t dstaddr = d1c->addr;
        dma_channel_config ctrl = d1c->ctrl;

        valid_params_if(PCH_DMACHAN,
                channel_config_get_transfer_data_size(ctrl) == DMA_SIZE_8);

        dmachan_link_t *txl = &tx->link;
        dmachan_link_cmd_set_zero(txl);
        txl->dmaid = dmaid;
        txl->dmairqix = d1c->dmairqix;
        channel_config_set_read_increment(&ctrl, true);
        channel_config_set_chain_to(&ctrl, dmaid);
        dma_channel_set_write_addr(dmaid, (void*)dstaddr, false);
        dma_channel_set_config(dmaid, &ctrl, false);
}
