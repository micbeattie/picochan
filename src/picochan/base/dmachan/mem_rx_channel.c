/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"

static void mem_start_dst_cmdbuf(dmachan_rx_channel_t *rx);
static void mem_start_dst_reset(dmachan_rx_channel_t *rx);
static void mem_start_dst_data(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
static void mem_start_dst_discard(dmachan_rx_channel_t *rx, uint32_t count);
static void mem_prep_dst_data_src_zeroes(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
static dmachan_irq_state_t mem_handle_rx_irq(dmachan_rx_channel_t *rx);

dmachan_rx_channel_ops_t dmachan_mem_rx_channel_ops = {
        .start_dst_cmdbuf = mem_start_dst_cmdbuf,
        .start_dst_reset = mem_start_dst_reset,
        .start_dst_data = mem_start_dst_data,
        .start_dst_discard = mem_start_dst_discard,
        .prep_dst_data_src_zeroes = mem_prep_dst_data_src_zeroes,
        .handle_rx_irq = mem_handle_rx_irq
};

static void __time_critical_func(mem_start_dst_cmdbuf)(dmachan_rx_channel_t *rx) {
        valid_params_if(PCH_DMACHAN,
                rx->u.mem.dst_state == DMACHAN_MEM_DST_IDLE);

        dmachan_tx_channel_t *txpeer = rx->u.mem.tx_peer;
        dmachan_link_t *rxl = &rx->link;
        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->u.mem.src_state;
        trace_dmachan_memstate(PCH_TRC_RT_DMACHAN_DST_CMDBUF_MEM,
                rxl, txpeer_mem_src_state);

        switch (txpeer_mem_src_state) {
        case DMACHAN_MEM_SRC_IDLE:
        case DMACHAN_MEM_SRC_DATA:
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_CMDBUF);
                break;
        case DMACHAN_MEM_SRC_CMDBUF:
                dmachan_link_t *txl = &txpeer->link;
                dmachan_link_cmd_copy(rxl, txl);
                trace_dmachan_cmd(PCH_TRC_RT_DMACHAN_MEMCHAN_RX_CMD, rxl);
                rxl->complete = true;
                dmachan_set_mem_src_state(txpeer, DMACHAN_MEM_SRC_IDLE);
                dmachan_set_link_irq_forced(txl, true);
                break;
        default:
                panic("mem_start_dst_cmdbuf unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}

static void __time_critical_func(mem_start_dst_reset)(dmachan_rx_channel_t *rx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_DST_RESET_MEM, &rx->link);
        // No reset action for now, go straight to receiving to cmdbuf
        mem_start_dst_cmdbuf(rx);
}

static void __time_critical_func(mem_start_dst_data)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                rx->u.mem.dst_state == DMACHAN_MEM_DST_IDLE);

        dmachan_tx_channel_t *txpeer = rx->u.mem.tx_peer;
        dmachan_link_t *rxl = &rx->link;
        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->u.mem.src_state;
        trace_dmachan_segment_memstate(PCH_TRC_RT_DMACHAN_DST_DATA_MEM,
                &rx->link, dstaddr, count, txpeer_mem_src_state);

        switch (txpeer_mem_src_state) {
        case DMACHAN_MEM_SRC_IDLE:
        case DMACHAN_MEM_SRC_CMDBUF:
                // SRC_CMDBUF can happen if the CU peer has sent its
                // Data command but not yet reached the tx complete
                // irq handler in which it'll move to SRC_DATA state.
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_DATA);
                dma_channel_set_write_addr(rxl->dmaid, (void*)dstaddr, false);
                dma_channel_set_trans_count(rxl->dmaid, count, false);
                break;
        case DMACHAN_MEM_SRC_DATA:
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_DATA);
                assert(dma_channel_get_reload_count(rxl->dmaid) == count);
                dma_channel_transfer_to_buffer_now(rxl->dmaid,
                        (void*)dstaddr, count);
                break;
        default:
                panic("mem_start_dst_data unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}

static void __time_critical_func(mem_start_dst_discard)(dmachan_rx_channel_t *rx, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                rx->u.mem.dst_state == DMACHAN_MEM_DST_IDLE);

        (void)count; // ignore count - we bypass doing any DMA transfer
        dmachan_tx_channel_t *txpeer = rx->u.mem.tx_peer;
        dmachan_link_t *rxl = &rx->link;
        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->u.mem.src_state;
        trace_dmachan_segment_memstate(PCH_TRC_RT_DMACHAN_DST_DISCARD_MEM,
                rxl, 0, count, txpeer_mem_src_state);

        switch (txpeer_mem_src_state) {
        case DMACHAN_MEM_SRC_IDLE:
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_DISCARD);
                break;
        case DMACHAN_MEM_SRC_DATA:
                dmachan_link_t *txl = &txpeer->link;
                rxl->complete = true;
                dmachan_set_mem_src_state(txpeer, DMACHAN_MEM_SRC_IDLE);
                dmachan_set_link_irq_forced(txl, true);
                break;
        default:
                panic("mem_start_dst_discard unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}

static void __time_critical_func(mem_prep_dst_data_src_zeroes)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        // for verification only
        dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_SRC_ZEROES);
}

static dmachan_irq_state_t __time_critical_func(mem_handle_rx_irq)(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        uint32_t status = mem_peer_lock();
        bool rx_irq_raised = dmachan_link_irq_raised(rxl);
        bool rx_irq_forced = dmachan_get_link_irq_forced(rxl);
        if (rx_irq_raised) {
                if (rx_irq_forced)
                        dmachan_set_link_irq_forced(rxl, false);
                else {
                        // propagate to peer tx channel
                        // (asymmetric: no corresponding tx -> rx trigger)
                        dmachan_tx_channel_t *txpeer = rx->u.mem.tx_peer;
                        if (txpeer) {
                                trace_dmachan(PCH_TRC_RT_DMACHAN_FORCE_IRQ,
                                        rxl);
                                dmachan_set_link_irq_forced(&txpeer->link,
                                        true);
                        }
                }

                rxl->complete = true;
                dmachan_ack_link_irq(rxl);
        }

        if (rxl->complete)
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_IDLE);

        if (rxl->resetting)
                dmachan_handle_rx_resetting(rx);

        mem_peer_unlock(status);
        return dmachan_make_irq_state(rx_irq_raised, rx_irq_forced,
                rxl->complete);
}
