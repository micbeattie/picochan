/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "cu_internal.h"
#include "cus_trace.h"

void __isr __time_critical_func(pch_cus_handle_dma_irq)() {
        uint irqnum = __get_current_exception() - VTABLE_FIRST_IRQ;
        pch_irq_index_t dmairqix = (pch_irq_index_t)(irqnum - DMA_IRQ_0);
        for (int i = 0; i < PCH_NUM_CUS; i++) {
                pch_cu_t *cu = pch_cus[i];
                if (cu == NULL || cu->dmairqix != dmairqix)
                        continue;

                pch_channel_t *ch = &cu->channel;
                if (!pch_channel_is_started(ch))
                        continue;

                pch_channel_handle_dma_irq(ch);
                if (ch->tx.link.complete || ch->rx.link.complete)
                        pch_cu_schedule_worker(cu);
        }
}
