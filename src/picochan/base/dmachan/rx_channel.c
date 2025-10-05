/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "picochan/dmachan.h"
#include "dmachan_internal.h"

// This handles two classes of channel: non-memchan and memchan.
// It's still just about better to keep this as a simplistic
// "if"-based dispatch based on rx->peer being set or not but as
// soon as we introduce a third type of channel, this'll be
// replaced with a typical generic "object method" dispatcher.

static void start_dst_cmdbuf_remote(dmachan_rx_channel_t *rx) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_DST_CMDBUF_REMOTE, &rx->link);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rx->link.dmaid, &ctrl, &rx->link.cmd,
                (void*)rx->srcaddr, DMACHAN_CMD_SIZE, true);
}

#if PCH_CONFIG_ENABLE_MEMCHAN
static void start_dst_cmdbuf_mem(dmachan_rx_channel_t *rx, dmachan_tx_channel_t *txpeer) {
        valid_params_if(PCH_DMACHAN,
                rx->mem_dst_state == DMACHAN_MEM_DST_IDLE);

        dmachan_link_t *rxl = &rx->link;
        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->mem_src_state;
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
                rxl->complete = true;
                dmachan_set_link_irq_forced(txl, true);
                break;
        default:
                panic("start_dst_cmdbuf_mem unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}
#endif

// Receive single characters at a time looking for DMACHAN_RESET_BYTE
// ('C') so that we can ignore any zero bytes due to Break conditions
// on a uart channel and resync with the sender.
static void start_dst_reset_remote(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan(PCH_TRC_RT_DMACHAN_DST_RESET_REMOTE, rxl);
        rxl->resetting = true;
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rx->link.dmaid, &ctrl, &rx->link.cmd,
                (void*)rx->srcaddr, 1, true);
}

#if PCH_CONFIG_ENABLE_MEMCHAN
static void start_dst_reset_mem(dmachan_rx_channel_t *rx, dmachan_tx_channel_t *txpeer) {
        trace_dmachan(PCH_TRC_RT_DMACHAN_DST_RESET_MEM, &rx->link);
        // No reset action for now, go straight to receiving to cmdbuf
        start_dst_cmdbuf_mem(rx, txpeer);
}
#endif

static void start_dst_data_remote(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        dmachan_link_t *rxl = &rx->link;
        trace_dmachan_segment(PCH_TRC_RT_DMACHAN_DST_DATA_REMOTE,
                rxl, dstaddr, count);
        dma_channel_config ctrl = rx->ctrl;
        channel_config_set_write_increment(&ctrl, true);
        dma_channel_configure(rxl->dmaid, &ctrl, (void*)dstaddr,
                (void*)rx->srcaddr, count, true);
}

#if PCH_CONFIG_ENABLE_MEMCHAN
static void start_dst_data_mem(dmachan_rx_channel_t *rx, dmachan_tx_channel_t *txpeer, uint32_t dstaddr, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                rx->mem_dst_state == DMACHAN_MEM_DST_IDLE);

        dmachan_link_t *rxl = &rx->link;
        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->mem_src_state;
        trace_dmachan_segment_memstate(PCH_TRC_RT_DMACHAN_DST_DATA_MEM,
                &rx->link, dstaddr, count, txpeer_mem_src_state);

        switch (txpeer_mem_src_state) {
        case DMACHAN_MEM_SRC_IDLE:
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
                panic("start_dst_data unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}
#endif

static void start_dst_discard_remote(dmachan_rx_channel_t *rx, uint32_t count) {
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

#if PCH_CONFIG_ENABLE_MEMCHAN
static void start_dst_discard_mem(dmachan_rx_channel_t *rx, dmachan_tx_channel_t *txpeer, uint32_t count) {
        valid_params_if(PCH_DMACHAN,
                rx->mem_dst_state == DMACHAN_MEM_DST_IDLE);

        (void)count; // ignore count - we bypass doing any DMA transfer
        dmachan_link_t *rxl = &rx->link;
        uint32_t status = mem_peer_lock();

        dmachan_mem_src_state_t txpeer_mem_src_state = txpeer->mem_src_state;
        trace_dmachan_segment_memstate(PCH_TRC_RT_DMACHAN_DST_DISCARD_MEM,
                rxl, 0, count, txpeer_mem_src_state);

        switch (txpeer_mem_src_state) {
        case DMACHAN_MEM_SRC_IDLE:
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_DISCARD);
                break;
        case DMACHAN_MEM_SRC_DATA:
                dmachan_link_t *txl = &txpeer->link;
                rxl->complete = true;
                dmachan_set_link_irq_forced(txl, true);
                break;
        default:
                panic("start_dst_discard unexpected txpeer->mem_src_state");
                // NOTREACHED
                break;
        }

        mem_peer_unlock(status);
}
#endif

void dmachan_init_rx_channel(dmachan_rx_channel_t *rx, dmachan_1way_config_t *d1c) {
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
        rxl->dmairqix = d1c->dmairqix;
        dma_channel_set_config(dmaid, &ctrl, false);
}

void __time_critical_func(dmachan_start_dst_cmdbuf)(dmachan_rx_channel_t *rx) {
        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
        if (txpeer != NULL) {
                start_dst_cmdbuf_mem(rx, txpeer);
                return;
        }
#else
        assert(!txpeer);
        (void)txpeer;
#endif
        start_dst_cmdbuf_remote(rx);
}

void __time_critical_func(dmachan_start_dst_reset)(dmachan_rx_channel_t *rx) {
        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
        if (txpeer != NULL) {
                start_dst_reset_mem(rx, txpeer);
                return;
        }
#else
        assert(!txpeer);
        (void)txpeer;
#endif
        start_dst_reset_remote(rx);
}

void __time_critical_func(dmachan_start_dst_data)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
        if (txpeer != NULL) {
                start_dst_data_mem(rx, txpeer, dstaddr, count);
                return;
        }
#else
        assert(!txpeer);
        (void)txpeer;
#endif
        start_dst_data_remote(rx, dstaddr, count);
}

void __time_critical_func(dmachan_start_dst_discard)(dmachan_rx_channel_t *rx, uint32_t count) {
        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
        if (txpeer != NULL) {
                start_dst_discard_mem(rx, txpeer, count);
                return;
        }
#else
        assert(!txpeer);
        (void)txpeer;
#endif
        start_dst_discard_remote(rx, count);
}

void __time_critical_func(dmachan_start_dst_data_src_zeroes)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
        if (txpeer != NULL)
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_SRC_ZEROES); // for verification only
#else
        assert(!txpeer);
        (void)txpeer;
#endif

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

static void dmachan_handle_rx_resetting(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        rxl->complete = false; // don't pass on to channel handler

        if (rxl->cmd.buf[0] != DMACHAN_RESET_BYTE) {
                dmachan_dropped_reset_byte_count++;
                dmachan_start_dst_reset(rx);
                return;
        }

        // Found the synchronising "reset" byte - ready to
        // receive commands
        rxl->resetting = false;
        dmachan_start_dst_cmdbuf(rx);
}

dmachan_irq_state_t __time_critical_func(dmachan_handle_rx_irq)(dmachan_rx_channel_t *rx) {
        dmachan_link_t *rxl = &rx->link;
        uint32_t status = mem_peer_lock();
        bool rx_irq_raised = dmachan_link_irq_raised(rxl);
        bool rx_irq_forced = dmachan_get_link_irq_forced(rxl);
        if (rx_irq_raised) {
                if (rx_irq_forced)
                        dmachan_set_link_irq_forced(rxl, false);
                else {
                        // If memchan, propagate to peer tx channel
                        // (asymmetric: no corresponding tx -> rx trigger)
                        dmachan_tx_channel_t *txpeer = rx->mem_tx_peer;
#if PCH_CONFIG_ENABLE_MEMCHAN
                        if (txpeer) {
                                dmachan_set_link_irq_forced(&txpeer->link,
                                        true);
                        }
#else
                        assert(!txpeer);
                        (void)txpeer;
#endif
                }

                rxl->complete = true;
                dmachan_ack_link_irq(rxl);
        }

#if PCH_CONFIG_ENABLE_MEMCHAN
        if (rxl->complete)
                dmachan_set_mem_dst_state(rx, DMACHAN_MEM_DST_IDLE);
#endif

        if (rxl->resetting)
                dmachan_handle_rx_resetting(rx);

        mem_peer_unlock(status);
        return dmachan_make_irq_state(rx_irq_raised, rx_irq_forced,
                rxl->complete);
}
