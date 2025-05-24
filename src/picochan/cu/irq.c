/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "cu_internal.h"
#include "cus_trace.h"

static void cus_handle_dma_irq_cu(pch_cu_t *cu) {
        dmachan_tx_channel_t *tx = &cu->tx_channel;
        bool tx_irq_raised = dmachan_link_irq_raised(&tx->link);

        dmachan_rx_channel_t *rx = &cu->rx_channel;
        bool rx_irq_raised = dmachan_link_irq_raised(&rx->link);

        trace_cus_cu_irq(PCH_TRC_RT_CUS_CU_IRQ, cu, cu->dmairqix,
                tx_irq_raised, rx_irq_raised);

        if (rx_irq_raised) {
                dmachan_ack_rx_irq(rx);
                cus_handle_rx_complete(cu);
        }

        if (tx_irq_raised) {
                dmachan_ack_tx_irq(tx);
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
                if (cu == NULL || !cu->started || cu->dmairqix != dmairqix)
                        continue;

                cus_handle_dma_irq_cu(cu);
        }
}
