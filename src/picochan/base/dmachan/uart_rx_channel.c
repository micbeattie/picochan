/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"

static void uart_start_dst_cmdbuf(dmachan_rx_channel_t *rx);
static void uart_start_dst_reset(dmachan_rx_channel_t *rx);
static void uart_start_dst_data(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
static void uart_start_dst_discard(dmachan_rx_channel_t *rx, uint32_t count);

dmachan_rx_channel_ops_t dmachan_uart_rx_channel_ops = {
        .start_dst_cmdbuf = uart_start_dst_cmdbuf,
        .start_dst_reset = uart_start_dst_reset,
        .start_dst_data = uart_start_dst_data,
        .start_dst_discard = uart_start_dst_discard,
        .handle_rx_irq = remote_handle_rx_irq
};

static void __time_critical_func(uart_start_dst_cmdbuf)(dmachan_rx_channel_t *rx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_DST_CMDBUF_REMOTE, &rx->link);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rx->link.dmaid, &ctrl, &rx->link.cmd,
                (void*)rx->srcaddr, DMACHAN_CMD_SIZE, true);
}

static void __time_critical_func(uart_start_dst_reset)(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan(PCH_TRC_RT_DMACHAN_DST_RESET_REMOTE, rxl);
        rxl->resetting = true;
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rx->link.dmaid, &ctrl, &rx->link.cmd,
                (void*)rx->srcaddr, 1, true);
}

static void __time_critical_func(uart_start_dst_data)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_DST_DATA_REMOTE,
                rxl, dstaddr, count);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rxl->dmaid, &ctrl, (void*)dstaddr,
                (void*)rx->srcaddr, count, true);
}

static void __time_critical_func(uart_start_dst_discard)(dmachan_rx_channel_t *rx, uint32_t count) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_DST_DISCARD_REMOTE,
                rxl, 0, count);
        // We discard data by copying it into the 4-byte command buffer
        // (without incrementing the destination address). At the moment,
        // everything uses DataSize8 but if we plumb through choice of
        // DMA size then we can discard 4 bytes of data at a time.
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, false);
        dma_channel_configure(rxl->dmaid, &ctrl, &rxl->cmd,
                (void*)rx->srcaddr, count, true);
}

void dmachan_init_uart_rx_channel(dmachan_rx_channel_t *rx, dmachan_1way_config_t *d1c) {
        dmachan_init_rx_channel(rx, d1c, &dmachan_uart_rx_channel_ops);
        dmachan_set_link_dma_irq_enabled(&rx->link, true);
}
