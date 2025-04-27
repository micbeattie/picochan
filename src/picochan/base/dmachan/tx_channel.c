/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include <string.h>
#include "picochan/dmachan.h"
#include "mem_peer.h"

static inline void set_mem_src_state(dmachan_tx_channel_t *tx, enum dmachan_mem_src_state new_state) {
        valid_params_if(PCH_DMACHAN,
                new_state == DMACHAN_MEM_SRC_IDLE
                || tx->mem_src_state == DMACHAN_MEM_SRC_IDLE);

        tx->mem_src_state = new_state;
}

static void start_src_cmdbuf_remote(dmachan_tx_channel_t *tx) {
        dma_channel_transfer_from_buffer_now(tx->dmaid,
                tx->cmdbuf, CMDBUF_SIZE);
}

static void start_src_cmdbuf_mem(dmachan_tx_channel_t *tx, dmachan_rx_channel_t *rxpeer) {
        valid_params_if(PCH_DMACHAN,
                tx->mem_src_state == DMACHAN_MEM_SRC_IDLE);

        uint32_t saved_irq = mem_peer_lock();

        switch (rxpeer->mem_dst_state) {
        case DMACHAN_MEM_DST_IDLE:
                set_mem_src_state(tx, DMACHAN_MEM_SRC_CMDBUF);
                trigger_irq(rxpeer->dmaid); // triggers for us (same dmaid) too
                break;

        case DMACHAN_MEM_DST_CMDBUF:
                memcpy(rxpeer->cmdbuf, tx->cmdbuf, CMDBUF_SIZE);
                trigger_irq(rxpeer->dmaid); // triggers for us (same dmaid) too
                break;

        default:
                panic("start_src_cmdbuf_mem unexpected rxpeer->mem_dst_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(saved_irq);
}

static void start_src_data_remote(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count) {
        dma_channel_transfer_from_buffer_now(tx->dmaid,
                (void*)srcaddr, count);
}

static void start_src_data_mem(dmachan_tx_channel_t *tx, dmachan_rx_channel_t *rxpeer, uint32_t srcaddr, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                tx->mem_src_state == DMACHAN_MEM_SRC_IDLE);

        uint32_t saved_irq = mem_peer_lock();

        switch (rxpeer->mem_dst_state) {
        case DMACHAN_MEM_DST_IDLE:
        case DMACHAN_MEM_DST_CMDBUF:
                set_mem_src_state(tx, DMACHAN_MEM_SRC_DATA);
                dma_channel_set_read_addr(tx->dmaid,
                        (void*)srcaddr, false);
                dma_channel_set_trans_count(tx->dmaid, count, false);
                break;

        case DMACHAN_MEM_DST_DATA:
                assert(dma_channel_get_transfer_count(tx->dmaid) == count);
                set_mem_src_state(tx, DMACHAN_MEM_SRC_DATA);
                // Peer has set its side but in order to raise the IRQ to
                // notify us, it had to write a zero control register so we
                // need to write a full one now to do the copy properly.
                dma_channel_set_config(tx->dmaid, &rxpeer->ctrl, false);
                dma_channel_transfer_from_buffer_now(tx->dmaid,
                        (void*)srcaddr, count);
                break;

        case DMACHAN_MEM_DST_DISCARD:
                trigger_irq(rxpeer->dmaid); // triggers for us (same dmaid) too
                break;

        default:
                panic("start_src_data unexpected rxpeer->mem_dst_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(saved_irq);
}

void dmachan_init_tx_channel(dmachan_tx_channel_t *tx, dmachan_1way_config_t *d1c) {
        pch_dmaid_t dmaid = d1c->dmaid;
        uint32_t dstaddr = d1c->addr;
        dma_channel_config ctrl = d1c->ctrl;

        valid_params_if(PCH_DMACHAN,
                !channel_config_get_incr_write(ctrl));
        valid_params_if(PCH_DMACHAN,
                channel_config_get_transfer_data_size(ctrl) == DMA_SIZE_8);

        memset(&tx->cmdbuf, 0, CMDBUF_SIZE);
        tx->dmaid = dmaid;
        channel_config_set_read_increment(&ctrl, true);
        channel_config_set_chain_to(&ctrl, dmaid);
        dma_channel_set_write_addr(dmaid, (void*)dstaddr, false);
        dma_channel_set_config(dmaid, &ctrl, false);
}

void __time_critical_func(dmachan_start_src_cmdbuf)(dmachan_tx_channel_t *tx) {
        dmachan_rx_channel_t *rxpeer = tx->mem_rx_peer;
        if (rxpeer != NULL)
                start_src_cmdbuf_mem(tx, rxpeer);
        else
                start_src_cmdbuf_remote(tx);
}

void __time_critical_func(dmachan_start_src_data)(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count) {
        dmachan_rx_channel_t *rxpeer = tx->mem_rx_peer;
        if (rxpeer != NULL)
                start_src_data_mem(tx, rxpeer, srcaddr, count);
        else
                start_src_data_remote(tx, srcaddr, count);
}
