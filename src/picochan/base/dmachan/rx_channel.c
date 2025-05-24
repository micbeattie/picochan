/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include <string.h>
#include "picochan/dmachan.h"
#include "dmachan_internal.h"

static void start_dst_cmdbuf_remote(dmachan_rx_channel_t *rx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_DST_CMDBUF_REMOTE, &rx->link);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rx->link.dmaid, &ctrl, rx->link.cmdbuf,
                (void*)rx->srcaddr, CMDBUF_SIZE, true);
}

static void start_dst_cmdbuf_mem(dmachan_rx_channel_t *rx, dmachan_tx_channel_t *txpeer) {
        valid_params_if(PCH_DMACHAN,
                rx->mem_dst_state == DMACHAN_MEM_DST_IDLE);

        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->mem_src_state;
        trace_dmachan_memstate(PCH_TRC_RT_DMACHAN_DST_CMDBUF_MEM,
                &rx->link, txpeer_mem_src_state);

        switch (txpeer_mem_src_state) {
        case DMACHAN_MEM_SRC_IDLE:
        case DMACHAN_MEM_SRC_DATA:
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_CMDBUF);
                break;
        case DMACHAN_MEM_SRC_CMDBUF:
                memcpy(rx->link.cmdbuf, txpeer->link.cmdbuf, CMDBUF_SIZE);
                trigger_irq(rx->link.dmaid); // trigger for txpeer (same dmaid) too
                break;
        default:
                panic("start_dst_cmdbuf_mem unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}

static void start_dst_data_remote(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_DST_DATA_REMOTE,
                &rx->link, dstaddr, count);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rx->link.dmaid, &ctrl, (void*)dstaddr,
                (void*)rx->srcaddr, count, true);
}

static void start_dst_data_mem(dmachan_rx_channel_t *rx, dmachan_tx_channel_t *txpeer, uint32_t dstaddr, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                rx->mem_dst_state == DMACHAN_MEM_DST_IDLE);

        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->mem_src_state;
        trace_dmachan_segment_memstate(PCH_TRC_RT_DMACHAN_DST_DATA_MEM,
                &rx->link, dstaddr, count, txpeer_mem_src_state);

        switch (txpeer_mem_src_state) {
        case DMACHAN_MEM_SRC_IDLE:
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_DATA);
                dma_channel_set_write_addr(rx->link.dmaid, (void*)dstaddr, false);
                dma_channel_set_trans_count(rx->link.dmaid, count, false);
                trigger_irq(rx->link.dmaid); // trigger for txpeer (same dmaid) too
                break;
        case DMACHAN_MEM_SRC_DATA:
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_DATA);
                assert(dma_channel_get_reload_count(rx->link.dmaid) == count);
                // Peer has set its side but in order to raise the IRQ to
                // notify us, it had to write a zero control register so we
                // need to write a full one now to do the copy properly.
                dma_channel_set_config(rx->link.dmaid, &rx->ctrl, false);
                dma_channel_transfer_to_buffer_now(rx->link.dmaid,
                        (void*)dstaddr, count);
                break;
        default:
                panic("start_dst_data unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}

static void start_dst_discard_remote(dmachan_rx_channel_t *rx, uint32_t count) {
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_DST_DISCARD_REMOTE,
                &rx->link, 0, count);
        // We discard data by copying it into the 4-byte command buffer
        // (without incrementing the destination address). At the moment,
        // everything uses DataSize8 but if we plumb through choice of
        // DMA size then we can discard 4 bytes of data at a time.
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, false);
        dma_channel_configure(rx->link.dmaid, &ctrl, rx->link.cmdbuf,
                (void*)rx->srcaddr, count, true);
}

static void start_dst_discard_mem(dmachan_rx_channel_t *rx, dmachan_tx_channel_t *txpeer, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                rx->mem_dst_state == DMACHAN_MEM_DST_IDLE);

        (void)count; // ignore count - we bypass doing any DMA transfer
        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->mem_src_state;
        trace_dmachan_segment_memstate(PCH_TRC_RT_DMACHAN_DST_DISCARD_MEM,
                &rx->link, 0, count, txpeer_mem_src_state);

        switch (txpeer_mem_src_state) {
        case DMACHAN_MEM_SRC_IDLE:
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_DISCARD);
                break;
        case DMACHAN_MEM_SRC_DATA:
                trigger_irq(rx->link.dmaid); // trigger for txpeer (same dmaid) too
                break;
        default:
                panic("start_dst_discard unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}

void dmachan_init_rx_channel(dmachan_rx_channel_t *rx, dmachan_1way_config_t *d1c) {
        pch_dmaid_t dmaid = d1c->dmaid;
        uint32_t srcaddr = d1c->addr;
        dma_channel_config ctrl = d1c->ctrl;

        valid_params_if(PCH_DMACHAN,
                channel_config_get_transfer_data_size(ctrl) == DMA_SIZE_8);

        memset(&rx->link.cmdbuf, 0, CMDBUF_SIZE);
        rx->srcaddr = srcaddr;
        channel_config_set_chain_to(&ctrl, dmaid);
        rx->ctrl = ctrl;
        rx->link.dmaid = dmaid;
        dma_channel_set_config(dmaid, &ctrl, false);
}

void __time_critical_func(dmachan_start_dst_cmdbuf)(dmachan_rx_channel_t *rx) {
        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
        if (txpeer != NULL)
                start_dst_cmdbuf_mem(rx, txpeer);
        else
                start_dst_cmdbuf_remote(rx);
}

void __time_critical_func(dmachan_start_dst_data)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
        if (txpeer != NULL)
                start_dst_data_mem(rx, txpeer, dstaddr, count);
        else
                start_dst_data_remote(rx, dstaddr, count);
}

void __time_critical_func(dmachan_start_dst_discard)(dmachan_rx_channel_t *rx, uint32_t count) {
        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
        if (txpeer != NULL)
                start_dst_discard_mem(rx, txpeer, count);
        else
                start_dst_discard_remote(rx, count);
}

void __time_critical_func(dmachan_start_dst_data_src_zeroes)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        if (rx->mem_tx_peer != NULL)
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_SRC_ZEROES); // for verification only

        // We set 4 bytes of zeroes to use as DMA source. At the moment,
        // everything uses DataSize8 but if we plumb through choice of
        // DMA size then we can write 4 bytes of zeroes at a time.
        memset(rx->link.cmdbuf, 0, 4);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_read_increment(&ctrl, false);
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rx->link.dmaid, &ctrl, (void*)dstaddr,
                rx->link.cmdbuf, count, true);
}
