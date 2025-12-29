/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "memchan_internal.h"

static void mem_start_src_cmdbuf(dmachan_tx_channel_t *tx);
static void mem_write_src_reset(dmachan_tx_channel_t *tx);
static void mem_start_src_data(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count);
static dmachan_irq_state_t mem_handle_tx_dma_irq(dmachan_tx_channel_t *tx);

dmachan_tx_channel_ops_t dmachan_mem_tx_channel_ops = {
        .start_src_cmdbuf = mem_start_src_cmdbuf,
        .write_src_reset = mem_write_src_reset,
        .start_src_data = mem_start_src_data,
        .handle_tx_dma_irq = mem_handle_tx_dma_irq
};

static void __time_critical_func(mem_start_src_cmdbuf)(dmachan_tx_channel_t *tx) {
        valid_params_if(PCH_DMACHAN,
                tx->u.mem.src_state == DMACHAN_MEM_SRC_IDLE);

        dmachan_rx_channel_t *rxpeer = tx->u.mem.rx_peer;
        dmachan_link_t *txl = &tx->link;
        uint32_t saved_irq = mem_peer_lock();

        dmachan_mem_dst_state_t rxpeer_mem_dst_state = rxpeer->u.mem.dst_state;
        trace_dmachan_memstate(PCH_TRC_RT_DMACHAN_SRC_CMDBUF_MEM,
                txl, rxpeer_mem_dst_state);

        switch (rxpeer_mem_dst_state) {
        case DMACHAN_MEM_DST_IDLE:
                dmachan_set_mem_src_state(tx, DMACHAN_MEM_SRC_CMDBUF);
                break;

        case DMACHAN_MEM_DST_CMDBUF:
                dmachan_link_t *rxl = &rxpeer->link;
                dmachan_link_cmd_copy(rxl, txl);
                trace_dmachan_cmd(PCH_TRC_RT_DMACHAN_MEMCHAN_TX_CMD, txl);
                txl->complete = true;
                dmachan_set_mem_dst_state(rxpeer, DMACHAN_MEM_DST_IDLE);
                dmachan_set_link_dma_irq_forced(rxl, true);
                break;

        default:
                panic("mem_start_src_cmdbuf unexpected rxpeer->mem_dst_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(saved_irq);
}

static void __time_critical_func(mem_write_src_reset)(dmachan_tx_channel_t *tx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_SRC_RESET_REMOTE, &tx->link);
        // Bypass DMA and write a single 32-bit word with low byte
        // DMACHAN_RESET_BYTE to the address in the DMA write address
        // register which is the address of the hardware transmit FIFO
        // for the channel
        dma_channel_hw_t *dmahw = dma_channel_hw_addr(tx->link.dmaid);
        *(uint32_t*)dmahw->write_addr = DMACHAN_RESET_BYTE;
}

static void __time_critical_func(mem_start_src_data)(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                tx->u.mem.src_state == DMACHAN_MEM_SRC_IDLE);

        dmachan_rx_channel_t *rxpeer = tx->u.mem.rx_peer;
        dmachan_link_t *txl = &tx->link;
        uint32_t saved_irq = mem_peer_lock();

        dmachan_mem_dst_state_t rxpeer_mem_dst_state = rxpeer->u.mem.dst_state;
        trace_dmachan_segment_memstate(PCH_TRC_RT_DMACHAN_SRC_DATA_MEM,
                txl, srcaddr, count, rxpeer_mem_dst_state);

        switch (rxpeer_mem_dst_state) {
        case DMACHAN_MEM_DST_IDLE:
        case DMACHAN_MEM_DST_CMDBUF:
                dmachan_set_mem_src_state(tx, DMACHAN_MEM_SRC_DATA);
                dma_channel_set_read_addr(txl->dmaid,
                        (void*)srcaddr, false);
                dma_channel_set_trans_count(txl->dmaid, count, false);
                break;

        case DMACHAN_MEM_DST_DATA:
                dmachan_set_mem_src_state(tx, DMACHAN_MEM_SRC_DATA);
                dma_channel_transfer_from_buffer_now(txl->dmaid,
                        (void*)srcaddr, count);
                break;

        case DMACHAN_MEM_DST_DISCARD:
                txl->complete = true;
                dmachan_set_mem_dst_state(rxpeer, DMACHAN_MEM_DST_IDLE);
                dmachan_set_link_dma_irq_forced(&rxpeer->link, true);
                break;

        default:
                panic("mem_start_src_data unexpected rxpeer->mem_dst_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(saved_irq);
}

static dmachan_irq_state_t __time_critical_func(mem_handle_tx_dma_irq)(dmachan_tx_channel_t *tx) {
        dmachan_link_t *txl = &tx->link;
        uint32_t saved_irq = mem_peer_lock();
        bool tx_irq_raised = dmachan_link_dma_irq_raised(txl);
        bool tx_irq_forced = dmachan_get_link_dma_irq_forced(txl);
        if (tx_irq_raised || tx_irq_forced) {
                txl->complete = true;
                dmachan_set_link_dma_irq_forced(txl, false);
                dmachan_ack_link_dma_irq(txl);
        }

        if (txl->complete)
                dmachan_set_mem_src_state(tx, DMACHAN_MEM_SRC_IDLE);

        mem_peer_unlock(saved_irq);
        return dmachan_make_irq_state(tx_irq_raised, tx_irq_forced,
                txl->complete);
}
