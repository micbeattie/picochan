/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "cu_internal.h"
#include "cus_trace.h"

typedef enum __attribute__((packed)) irq_index_config_state {
        DMAIRQIX_CONFIG_UNUSED = 0,     // UNUSED must be the 0 value
        DMAIRQIX_CONFIG_CONFIGURED,
        DMAIRQIX_CONFIG_MUST_NOT_USE,
} irq_index_config_state_t;

typedef struct irq_index_config {
        irq_index_config_state_t state;
        uint8_t                 core_num;
} irq_index_config_t;

irq_index_config_t irq_index_configs[NUM_DMA_IRQS];

static irq_index_config_t *get_irq_index_config(pch_irq_index_t irq_index) {
        assert(irq_index >= 0 && irq_index < NUM_DMA_IRQS);
        return &irq_index_configs[irq_index];
}

void pch_cus_ignore_irq_index_t(pch_irq_index_t irq_index) {
        assert(irq_index >= 0 && irq_index < NUM_DMA_IRQS);
        irq_index_config_t *dc = get_irq_index_config(irq_index);
        assert(dc->state != DMAIRQIX_CONFIG_CONFIGURED);
        dc->state = DMAIRQIX_CONFIG_MUST_NOT_USE;
}

static void trace_configure_irq_index(irq_num_t irqnum, int16_t order_priority_opt) {
        PCH_CUS_TRACE(PCH_TRC_RT_CUS_INIT_DMA_IRQ_HANDLER,
                ((struct pch_trdata_irq_handler){
                        .handler = (uint32_t)pch_cus_handle_dma_irq,
                        .order_priority = order_priority_opt,
                        .irqnum = (uint8_t)irqnum
                }));
}

static irq_num_t prepare_configure_irq_index(pch_irq_index_t irq_index) {
        assert(irq_index >= 0 && irq_index < NUM_DMA_IRQS);
        irq_index_config_t *dc = get_irq_index_config(irq_index);
        assert(dc->state == DMAIRQIX_CONFIG_UNUSED);
        irq_num_t irqnum = dma_get_irq_num((uint)irq_index);
        uint core_num = get_core_num();
        dc->core_num = (uint8_t)core_num;
        dc->state = DMAIRQIX_CONFIG_CONFIGURED;
        return irqnum;
}

void pch_cus_configure_irq_index_exclusive(pch_irq_index_t irq_index) {
        irq_num_t irqnum = prepare_configure_irq_index(irq_index);
        irq_set_exclusive_handler(irqnum, pch_cus_handle_dma_irq);
        irq_set_enabled(irqnum, true);
        trace_configure_irq_index(irqnum, -1);
}

void pch_cus_configure_irq_index_shared(pch_irq_index_t irq_index, uint8_t order_priority) {
        irq_num_t irqnum = prepare_configure_irq_index(irq_index);
        irq_add_shared_handler(irqnum, pch_cus_handle_dma_irq,
                order_priority);
        irq_set_enabled(irqnum, true);
        trace_configure_irq_index(irqnum, (int16_t)order_priority);
}

void pch_cus_configure_irq_index_shared_default(pch_irq_index_t irq_index) {
        pch_cus_configure_irq_index_shared(irq_index,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
}

pch_irq_index_t pch_cus_auto_configure_irq_index(bool required) {
        uint core_num = get_core_num();
        pch_irq_index_t first_unused = -1;

        for (pch_irq_index_t irq_index = 0; irq_index < NUM_DMA_IRQS; irq_index++) {
                irq_index_config_t *dc = get_irq_index_config(irq_index);
                if (dc->state == DMAIRQIX_CONFIG_CONFIGURED) {
                        if (dc->core_num == core_num)
                                return irq_index; // found one for our core
                } else if (dc->state == DMAIRQIX_CONFIG_UNUSED) {
                        if (first_unused == -1)
                                first_unused = irq_index;
                }
        }

        // Found no irq_index already configured for our core
        if (first_unused >= 0)
                pch_cus_configure_irq_index_shared_default(first_unused);
        else if (required)
                panic("no available DMA IRQ indexes");

        return first_unused;
}

void pch_cu_set_irq_index(pch_cu_t *cu, pch_irq_index_t irq_index) {
        assert(irq_index >= 0 && irq_index < NUM_DMA_IRQS);
        cu->irq_index = irq_index;
}

void __isr __time_critical_func(pch_cus_handle_dma_irq)() {
        uint irqnum = __get_current_exception() - VTABLE_FIRST_IRQ;
        pch_irq_index_t irq_index = (pch_irq_index_t)(irqnum - DMA_IRQ_0);
        for (int i = 0; i < PCH_NUM_CUS; i++) {
                pch_cu_t *cu = pch_cus[i];
                if (cu == NULL || cu->irq_index != irq_index)
                        continue;

                pch_channel_t *ch = &cu->channel;
                if (!pch_channel_is_started(ch))
                        continue;

                pch_channel_handle_dma_irq(ch);
                if (ch->tx.link.complete || ch->rx.link.complete)
                        pch_cu_schedule_worker(cu);
        }
}
