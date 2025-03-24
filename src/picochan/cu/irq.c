/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "cu_internal.h"

static void handle_dma_irq_cu(pch_dma_irq_index_t dmairqix, pch_cu_t *cu) {
        dmachan_rx_channel_t *rx = &cu->rx_channel;
        if (dmachan_rx_irq_raised(rx, dmairqix)) {
                dmachan_ack_rx_irq(rx, dmairqix);
                cus_handle_rx_complete(cu);
        }

        dmachan_tx_channel_t *tx = &cu->tx_channel;
        if (dmachan_tx_irq_raised(tx, dmairqix)) {
                dmachan_ack_tx_irq(tx, dmairqix);
                cus_handle_tx_complete(cu);
        }
}

void __isr __time_critical_func(pch_cus_handle_dma_irq)() {
        uint irqnum = __get_current_exception() - VTABLE_FIRST_IRQ;
	// TODO deal with getting the Irq information and acking
	// them in a batch instead of individually
        pch_dma_irq_index_t dmairqix = (pch_dma_irq_index_t)(irqnum - DMA_IRQ_0);
        for (int i = 0; i < NUM_CUS; i++) {
                pch_cu_t *cu = pch_cus[i];
                if (cu == NULL || !cu->enabled || cu->dmairqix != dmairqix)
                        continue;

                handle_dma_irq_cu(dmairqix, cu);
        }
}
