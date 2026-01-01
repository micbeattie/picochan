/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"

static void pio_start_dst_cmdbuf(dmachan_rx_channel_t *rx);
static void pio_start_dst_reset(dmachan_rx_channel_t *rx);
static void pio_start_dst_data(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
static void pio_start_dst_discard(dmachan_rx_channel_t *rx, uint32_t count);

dmachan_rx_channel_ops_t dmachan_pio_rx_channel_ops = {
        .start_dst_cmdbuf = pio_start_dst_cmdbuf,
        .start_dst_reset = pio_start_dst_reset,
        .start_dst_data = pio_start_dst_data,
        .start_dst_discard = pio_start_dst_discard,
        .handle_rx_irq = remote_handle_rx_irq
};

static void receive(dmachan_rx_channel_t *rx, bool write_inc, void *dst, uint32_t count) {
        pio_sm_put(rx->u.pio.pio, rx->u.pio.sm, 8 * count - 1);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, write_inc);
        dma_channel_configure(rx->link.dmaid, &ctrl, dst,
                (void*)rx->srcaddr, count, true);
}

static void __time_critical_func(pio_start_dst_cmdbuf)(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan(PCH_TRC_RT_DMACHAN_DST_CMDBUF_REMOTE, rxl);
        receive(rx, true, &rxl->cmd, DMACHAN_CMD_SIZE);
}

static void __time_critical_func(pio_start_dst_reset)(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan_byte(PCH_TRC_RT_DMACHAN_DST_RESET, rxl,
                DMACHAN_RESET_PROGRESSING);
        rxl->resetting = true;
        receive(rx, true, &rxl->cmd, 1);
}

static void __time_critical_func(pio_start_dst_data)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_DST_DATA_REMOTE,
                rxl, dstaddr, count);
        receive(rx, true, (void*)dstaddr, count);
}

static void __time_critical_func(pio_start_dst_discard)(dmachan_rx_channel_t *rx, uint32_t count) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_DST_DISCARD_REMOTE,
                rxl, 0, count);
        // We discard data by copying it into the 4-byte command buffer
        // (without incrementing the destination address). At the moment,
        // everything uses DataSize8 but if we plumb through choice of
        // DMA size then we can discard 4 bytes of data at a time.
        receive(rx, false, &rxl->cmd, count);
}
