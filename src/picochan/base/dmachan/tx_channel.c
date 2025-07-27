/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include <string.h>
#include "picochan/dmachan.h"
#include "dmachan_internal.h"
#include "dmachan_trace.h"

// This handles two classes of channel: non-memchan and memchan.
// It's still just about better to keep this as a simplistic
// "if"-based dispatch based on tx->peer being set or not but as
// soon as we introduce a third type of channel, this'll be
// replaced with a typical generic "object method" dispatcher.

static void start_src_cmdbuf_remote(dmachan_tx_channel_t *tx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_SRC_CMDBUF_REMOTE, &tx->link);
        dma_channel_transfer_from_buffer_now(tx->link.dmaid,
                &tx->link.cmd, DMACHAN_CMD_SIZE);
}

#if PCH_CONFIG_ENABLE_MEMCHAN
static void start_src_cmdbuf_mem(dmachan_tx_channel_t *tx, dmachan_rx_channel_t *rxpeer) {
        valid_params_if(PCH_DMACHAN,
                tx->mem_src_state == DMACHAN_MEM_SRC_IDLE);

        dmachan_link_t *txl = &tx->link;
        uint32_t saved_irq = mem_peer_lock();

        dmachan_mem_dst_state_t rxpeer_mem_dst_state = rxpeer->mem_dst_state;
        trace_dmachan_memstate(PCH_TRC_RT_DMACHAN_SRC_CMDBUF_MEM,
                txl, rxpeer_mem_dst_state);

        switch (rxpeer_mem_dst_state) {
        case DMACHAN_MEM_DST_IDLE:
                dmachan_set_mem_src_state(tx, DMACHAN_MEM_SRC_CMDBUF);
                break;

        case DMACHAN_MEM_DST_CMDBUF:
                dmachan_link_t *rxl = &rxpeer->link;
                dmachan_link_cmd_copy(rxl, txl);
                txl->complete = true;
                dmachan_set_link_irq_forced(rxl, true);
                break;

        default:
                panic("start_src_cmdbuf_mem unexpected rxpeer->mem_dst_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(saved_irq);
}
#endif

static void write_src_reset_remote(dmachan_tx_channel_t *tx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_SRC_RESET_REMOTE, &tx->link);
        // Bypass DMA and write a single 32-bit word with low byte
        // DMACHAN_RESET_BYTE to the address in the DMA write address
        // register which is the address of the hardware transmit FIFO
        // for the channel
        dma_channel_hw_t *dmahw = dma_channel_hw_addr(tx->link.dmaid);
        *(uint32_t*)dmahw->write_addr = DMACHAN_RESET_BYTE;
}

#if PCH_CONFIG_ENABLE_MEMCHAN
static void write_src_reset_mem(dmachan_tx_channel_t *tx, dmachan_rx_channel_t *rxpeer) {
        (void)rxpeer;
        trace_dmachan(PCH_TRC_RT_DMACHAN_SRC_RESET_MEM, &tx->link);
}
#endif

static void start_src_data_remote(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count) {
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_SRC_DATA_REMOTE,
                &tx->link, srcaddr, count);
        dma_channel_transfer_from_buffer_now(tx->link.dmaid,
                (void*)srcaddr, count);
}

#if PCH_CONFIG_ENABLE_MEMCHAN
static void start_src_data_mem(dmachan_tx_channel_t *tx, dmachan_rx_channel_t *rxpeer, uint32_t srcaddr, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                tx->mem_src_state == DMACHAN_MEM_SRC_IDLE);

        dmachan_link_t *txl = &tx->link;
        uint32_t saved_irq = mem_peer_lock();

        dmachan_mem_dst_state_t rxpeer_mem_dst_state = rxpeer->mem_dst_state;
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
                assert(dma_channel_get_transfer_count(txl->dmaid) == count);
                dmachan_set_mem_src_state(tx, DMACHAN_MEM_SRC_DATA);
                dma_channel_transfer_from_buffer_now(txl->dmaid,
                        (void*)srcaddr, count);
                break;

        case DMACHAN_MEM_DST_DISCARD:
                txl->complete = true;
                dmachan_set_link_irq_forced(&rxpeer->link, true);
                break;

        default:
                panic("start_src_data unexpected rxpeer->mem_dst_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(saved_irq);
}
#endif

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

void __time_critical_func(dmachan_start_src_cmdbuf)(dmachan_tx_channel_t *tx) {
        dmachan_rx_channel_t *rxpeer = tx->mem_rx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
        if (rxpeer != NULL) {
                start_src_cmdbuf_mem(tx, rxpeer);
                return;
        }
#else
        assert(!rxpeer);
        (void)rxpeer;
#endif
        start_src_cmdbuf_remote(tx);
}

void __time_critical_func(dmachan_write_src_reset)(dmachan_tx_channel_t *tx) {
        dmachan_rx_channel_t *rxpeer = tx->mem_rx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
        if (rxpeer != NULL) {
                write_src_reset_mem(tx, rxpeer);
                return;
        }
#else
        assert(!rxpeer);
        (void)rxpeer;
#endif
        write_src_reset_remote(tx);
}

void __time_critical_func(dmachan_start_src_data)(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count) {
        dmachan_rx_channel_t *rxpeer = tx->mem_rx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
        if (rxpeer != NULL) {
                start_src_data_mem(tx, rxpeer, srcaddr, count);
                return;
        }
#else
        assert(!rxpeer);
        (void)rxpeer;
#endif
        start_src_data_remote(tx, srcaddr, count);
}

dmachan_irq_state_t __time_critical_func(dmachan_handle_tx_irq)(dmachan_tx_channel_t *tx) {
        dmachan_link_t *txl = &tx->link;
        uint32_t saved_irq = mem_peer_lock();
        bool tx_irq_raised = dmachan_link_irq_raised(txl);
        bool tx_irq_forced = dmachan_get_link_irq_forced(txl);
        if (tx_irq_raised || tx_irq_forced) {
                txl->complete = true;
                dmachan_set_link_irq_forced(txl, false);
                dmachan_ack_link_irq(txl);
        }

#if PCH_CONFIG_ENABLE_MEMCHAN
        if (txl->complete)
                dmachan_set_mem_src_state(tx, DMACHAN_MEM_SRC_IDLE);
#endif

        mem_peer_unlock(saved_irq);
        return dmachan_make_irq_state(tx_irq_raised, tx_irq_forced,
                txl->complete);
}
