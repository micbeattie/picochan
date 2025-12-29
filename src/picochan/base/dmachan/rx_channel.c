/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "dmachan_internal.h"

void dmachan_init_rx_channel(dmachan_rx_channel_t *rx, dmachan_1way_config_t *d1c, const dmachan_rx_channel_ops_t *ops) {
        rx->ops = ops;
        pch_dmaid_t dmaid = d1c->dmaid;
        uint32_t srcaddr = d1c->addr;
        dma_channel_config ctrl = d1c->ctrl;

        valid_params_if(PCH_DMACHAN,
                channel_config_get_transfer_data_size(ctrl) == DMA_SIZE_8);

        dmachan_link_t *rxl = &rx->link;
        dmachan_link_cmd_set_zero(rxl);
        rx->srcaddr = srcaddr;
        channel_config_set_chain_to(&ctrl, dmaid);
        rx->ctrl = ctrl;
        rxl->dmaid = dmaid;
        rxl->irq_index = d1c->dmairqix;
        dma_channel_set_config(dmaid, &ctrl, false);
}

void __time_critical_func(dmachan_start_dst_data_src_zeroes)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        if (rx->ops->prep_dst_data_src_zeroes)
                rx->ops->prep_dst_data_src_zeroes(rx, dstaddr, count);

        // We set 4 bytes of zeroes to use as DMA source. At the moment,
        // everything uses DataSize8 but if we plumb through choice of
        // DMA size then we can write 4 bytes of zeroes at a time.
        dmachan_link_t *rxl = &rx->link;
        dmachan_link_cmd_set_zero(rxl);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_read_increment(&ctrl, false);
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rxl->dmaid, &ctrl, (void*)dstaddr,
                &rxl->cmd, count, true);
}

// Count drops of incorrect reset bytes for debugging
uint32_t dmachan_dropped_reset_byte_count;

void dmachan_handle_rx_resetting(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        rxl->complete = false; // don't pass on to channel handler

        if (rxl->cmd.buf[0] != DMACHAN_RESET_BYTE) {
                trace_dmachan_byte(PCH_TRC_RT_DMACHAN_DST_RESET, rxl,
                        DMACHAN_RESET_INVALID);
                dmachan_dropped_reset_byte_count++;
                dmachan_start_dst_reset(rx);
                return;
        }

        // Found the synchronising "reset" byte - ready to
        // receive commands
        rxl->resetting = false;
        trace_dmachan_byte(PCH_TRC_RT_DMACHAN_DST_RESET, rxl,
                DMACHAN_RESET_COMPLETE);
        dmachan_start_dst_cmdbuf(rx);
}

dmachan_irq_state_t __time_critical_func(remote_handle_rx_irq)(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        bool rx_irq_raised = dmachan_link_dma_irq_raised(rxl);
        if (rx_irq_raised) {
                rxl->complete = true;
                dmachan_ack_link_dma_irq(rxl);
        }

        if (rxl->resetting)
                dmachan_handle_rx_resetting(rx);

        return dmachan_make_irq_state(rx_irq_raised, false, rxl->complete);
}
