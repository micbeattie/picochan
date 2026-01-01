/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"

static void pio_start_src_cmdbuf(dmachan_tx_channel_t *tx);
static void pio_write_src_reset(dmachan_tx_channel_t *tx);
static void pio_start_src_data(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count);
static bool pio_handle_tx_pio_irq(dmachan_tx_channel_t *tx, uint irqnum);

dmachan_tx_channel_ops_t dmachan_pio_tx_channel_ops = {
        .start_src_cmdbuf = pio_start_src_cmdbuf,
        .write_src_reset = pio_write_src_reset,
        .start_src_data = pio_start_src_data,
        .handle_tx_pio_irq = pio_handle_tx_pio_irq,
};

static inline void pio_set_irqn_irqflag_enabled(PIO pio, uint irq_index, uint irqflag, bool enabled) {
        const pio_interrupt_source_t source = irqflag + PIO_INTR_SM0_LSB;
        pio_set_irqn_source_enabled(pio, irq_index, source, enabled);
}

static void send(dmachan_tx_channel_t *tx, const void *src, uint32_t count) {
        dmachan_pio_tx_channel_data_t *d = &tx->u.pio;
        PIO pio = d->pio;
        uint sm = d->sm;
        pch_irq_index_t irq_index = tx->link.irq_index;

        pio_sm_put(pio, sm, 8 * count - 1);
        dma_channel_transfer_from_buffer_now(tx->link.dmaid, src, count);
        // The tx SM raises the same irqflag number as its SM number
        pio_interrupt_clear(pio, sm);
        pio_set_irqn_irqflag_enabled(pio, irq_index, sm, true);
}

static void __time_critical_func(pio_start_src_cmdbuf)(dmachan_tx_channel_t *tx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_SRC_CMDBUF_REMOTE, &tx->link);
        send(tx, &tx->link.cmd, DMACHAN_CMD_SIZE);
}

static void __time_critical_func(pio_write_src_reset)(dmachan_tx_channel_t *tx) {
        // No reset action needed
}

static void __time_critical_func(pio_start_src_data)(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count) {
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_SRC_DATA_REMOTE,
                &tx->link, srcaddr, count);
        send(tx, (void*)srcaddr, count);
}

static bool __time_critical_func(pio_handle_tx_pio_irq)(dmachan_tx_channel_t *tx, uint irqnum) {
        dmachan_pio_tx_channel_data_t *d = &tx->u.pio;
        PIO pio = d->pio;
        uint sm = d->sm;
        pch_irq_index_t irq_index = tx->link.irq_index;

        // The tx SM raises the same irqflag number as its SM number
        if (PIO_IRQ_NUM(pio, irq_index) != irqnum)
                return false;

        if (!pio_interrupt_get(pio, sm))
                return false;

        pio_set_irqn_irqflag_enabled(pio, irq_index, sm, false);
        if (tx->link.resetting) {
                tx->link.resetting = false;
                return false; // discard completion interruption
        }

        return true;
}
