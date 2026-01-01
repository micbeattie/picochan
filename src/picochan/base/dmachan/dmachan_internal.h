/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_DMACHAN_DMACHAN_INTERNAL_H
#define _PCH_DMACHAN_DMACHAN_INTERNAL_H

#include "hardware/sync.h"
#include "picochan/dmachan.h"
#include "dmachan_trace.h"

// General Pico SDK-like DMA-related functions that aren't in the SDK
static inline enum dma_channel_transfer_size channel_config_get_transfer_data_size(dma_channel_config config) {
        uint size = (config.ctrl & DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS) >> DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
        return (enum dma_channel_transfer_size)size;
}

static inline bool dma_irqn_get_channel_forced(uint irq_index, uint channel) {
        invalid_params_if(HARDWARE_DMA, irq_index >= NUM_DMA_IRQS);
        check_dma_channel_param(channel);

        return dma_hw->irq_ctrl[irq_index].intf & (1u << channel);
}

static inline void dma_irqn_set_channel_forced(uint irq_index, uint channel, bool forced) {
        invalid_params_if(HARDWARE_DMA, irq_index >= NUM_DMA_IRQS);

        if (forced)
                hw_set_bits(&dma_hw->irq_ctrl[irq_index].intf, 1u << channel);
        else
                hw_clear_bits(&dma_hw->irq_ctrl[irq_index].intf, 1u << channel);
}

// DMA configuration for one direction (tx or rx) of a dmachan channel
typedef struct dmachan_1way_config {
        uint32_t                addr;
        dma_channel_config      ctrl;
        pch_dmaid_t             dmaid;
        pch_irq_index_t     dmairqix;
} dmachan_1way_config_t;

static inline dmachan_1way_config_t dmachan_1way_config_make(pch_dmaid_t dmaid, uint32_t addr, dma_channel_config ctrl, pch_irq_index_t dmairqix) {
        return ((dmachan_1way_config_t){
                .addr = addr,
                .ctrl = ctrl,
                .dmaid = dmaid,
                .dmairqix = dmairqix
        });
}

static inline dmachan_1way_config_t dmachan_1way_config_claim(uint32_t addr, dma_channel_config ctrl, pch_irq_index_t dmairqix) {
        pch_dmaid_t dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        return dmachan_1way_config_make(dmaid, addr, ctrl, dmairqix);
}

// DMA configuration for both directions (tx and rx) of a dmachan
// channel
typedef struct dmachan_config {
        dmachan_1way_config_t   tx;
        dmachan_1way_config_t   rx;
} dmachan_config_t;

static inline void dmachan_set_link_dma_irq_enabled(dmachan_link_t *l, bool enabled) {
        pch_irq_index_t dmairqix = l->irq_index;
        assert(dmairqix >= 0 && dmairqix < NUM_DMA_IRQS);
        dma_irqn_set_channel_enabled(dmairqix, l->dmaid, enabled);
}

static inline bool dmachan_link_dma_irq_raised(dmachan_link_t *l) {
        return dma_irqn_get_channel_status(l->irq_index, l->dmaid);
};

static inline bool dmachan_get_link_dma_irq_forced(dmachan_link_t *l) {
        return dma_irqn_get_channel_forced(l->irq_index, l->dmaid);
}

static inline void dmachan_set_link_dma_irq_forced(dmachan_link_t *l, bool forced) {
        dma_irqn_set_channel_forced(l->irq_index, l->dmaid, forced);
}

static inline void dmachan_ack_link_dma_irq(dmachan_link_t *l) {
        dma_irqn_acknowledge_channel(l->irq_index, l->dmaid);
}

static inline dmachan_irq_state_t dmachan_make_irq_state(bool raised, bool forced, bool complete) {
        return ((dmachan_irq_state_t)raised)
                | ((dmachan_irq_state_t)forced) << 1
                | ((dmachan_irq_state_t)complete) << 2;
}

dmachan_irq_state_t remote_handle_rx_irq(dmachan_rx_channel_t *rx);
void dmachan_handle_rx_resetting(dmachan_rx_channel_t *rx);

void dmachan_init_tx_channel(dmachan_tx_channel_t *tx, dmachan_1way_config_t *d1c, const dmachan_tx_channel_ops_t *ops);
void dmachan_init_rx_channel(dmachan_rx_channel_t *rx, dmachan_1way_config_t *d1c, const dmachan_rx_channel_ops_t *ops);

extern dmachan_rx_channel_ops_t dmachan_mem_rx_channel_ops;
extern dmachan_tx_channel_ops_t dmachan_mem_tx_channel_ops;
extern dmachan_rx_channel_ops_t dmachan_uart_rx_channel_ops;
extern dmachan_tx_channel_ops_t dmachan_uart_tx_channel_ops;
extern dmachan_rx_channel_ops_t dmachan_pio_rx_channel_ops;
extern dmachan_tx_channel_ops_t dmachan_pio_tx_channel_ops;

#endif
