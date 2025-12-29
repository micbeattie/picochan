/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"

static inline void trace_dma_irq(pch_channel_t *ch, pch_irq_index_t irq_index, uint8_t tx_irq_state, uint8_t rx_irq_state) {
        PCH_TRC_WRITE(ch->tx.link.bs, pch_channel_is_traced(ch),
                PCH_TRC_RT_DMACHAN_DMA_IRQ, ((struct pch_trdata_id_irq){
                        .id = ch->id,
                        .irq_index = irq_index,
                        .tx_state = tx_irq_state << 4
                                | ch->tx.u.mem.src_state,
                        .rx_state = rx_irq_state << 4
                                | ch->rx.u.mem.dst_state
                }));
}

static inline dmachan_irq_state_t handle_tx_dma_irq(dmachan_tx_channel_t *tx) {
        if (tx->ops->handle_tx_dma_irq)
                return tx->ops->handle_tx_dma_irq(tx);

        return 0;
}

static inline dmachan_irq_state_t handle_rx_irq(dmachan_rx_channel_t *rx) {
        return rx->ops->handle_rx_irq(rx);
}

void __time_critical_func(pch_channel_handle_dma_irq)(pch_channel_t *ch) {
        dmachan_irq_state_t tx_state = handle_tx_dma_irq(&ch->tx);
        dmachan_irq_state_t rx_state = handle_rx_irq(&ch->rx);

        trace_dma_irq(ch, ch->tx.link.irq_index, tx_state, rx_state);
}
