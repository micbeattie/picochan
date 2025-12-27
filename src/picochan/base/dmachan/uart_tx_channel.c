/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"

static void uart_start_src_cmdbuf(dmachan_tx_channel_t *tx);
static void uart_write_src_reset(dmachan_tx_channel_t *tx);
static void uart_start_src_data(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count);
static dmachan_irq_state_t uart_handle_tx_irq(dmachan_tx_channel_t *tx);

dmachan_tx_channel_ops_t dmachan_uart_tx_channel_ops = {
        .start_src_cmdbuf = uart_start_src_cmdbuf,
        .write_src_reset = uart_write_src_reset,
        .start_src_data = uart_start_src_data,
        .handle_tx_irq = uart_handle_tx_irq
};

static void __time_critical_func(uart_start_src_cmdbuf)(dmachan_tx_channel_t *tx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_SRC_CMDBUF_REMOTE, &tx->link);
        dma_channel_transfer_from_buffer_now(tx->link.dmaid,
                &tx->link.cmd, DMACHAN_CMD_SIZE);
}

static void __time_critical_func(uart_write_src_reset)(dmachan_tx_channel_t *tx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_SRC_RESET_REMOTE, &tx->link);
        // Bypass DMA and write a single 32-bit word with low byte
        // DMACHAN_RESET_BYTE to the address in the DMA write address
        // register which is the address of the hardware transmit FIFO
        // for the channel
        dma_channel_hw_t *dmahw = dma_channel_hw_addr(tx->link.dmaid);
        *(uint32_t*)dmahw->write_addr = DMACHAN_RESET_BYTE;
}

static void __time_critical_func(uart_start_src_data)(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count) {
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_SRC_DATA_REMOTE,
                &tx->link, srcaddr, count);
        dma_channel_transfer_from_buffer_now(tx->link.dmaid,
                (void*)srcaddr, count);
}

static dmachan_irq_state_t __time_critical_func(uart_handle_tx_irq)(dmachan_tx_channel_t *tx) {
        dmachan_link_t *txl = &tx->link;
        bool tx_irq_raised = dmachan_link_irq_raised(txl);
        if (tx_irq_raised) {
                txl->complete = true;
                dmachan_ack_link_irq(txl);
        }

        return dmachan_make_irq_state(tx_irq_raised, false, txl->complete);
}

void dmachan_init_uart_tx_channel(dmachan_tx_channel_t *tx, dmachan_1way_config_t *d1c) {
        dmachan_init_tx_channel(tx, d1c, &dmachan_uart_tx_channel_ops);
        dmachan_set_link_irq_enabled(&tx->link, true);
}
