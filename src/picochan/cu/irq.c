/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "cu_internal.h"
#include "cus_trace.h"

typedef enum __attribute__((packed)) irq_index_config_state {
        IRQIX_UNUSED = 0,     // UNUSED must be the 0 value
        IRQIX_CLAIMED,
        IRQIX_MUST_NOT_USE,
} irq_index_config_state_t;

#define NUM_IRQ_INDEXES NUM_DMA_IRQS

typedef struct irq_index_config {
        irq_index_config_state_t state;
        uint8_t                 core_num;
        bool                    dma_irq_configured;
} irq_index_config_t;

irq_index_config_t irq_index_configs[NUM_IRQ_INDEXES];

static irq_index_config_t *get_irq_index_config(pch_irq_index_t irq_index) {
        assert(irq_index >= 0 && irq_index < NUM_IRQ_INDEXES);
        return &irq_index_configs[irq_index];
}

void pch_cus_ignore_irq_index_t(pch_irq_index_t irq_index) {
        assert(irq_index >= 0 && irq_index < NUM_IRQ_INDEXES);
        irq_index_config_t *ic = get_irq_index_config(irq_index);
        assert(ic->state != IRQIX_CLAIMED);
        ic->state = IRQIX_MUST_NOT_USE;
}

static void trace_configure_irq_handler(pch_trc_record_type_t rt, irq_num_t irqnum, irq_handler_t handler, int16_t order_priority_opt) {
        PCH_CUS_TRACE(rt, ((struct pch_trdata_irq_handler){
                        .handler = (uint32_t)handler,
                        .order_priority = order_priority_opt,
                        .irqnum = (uint8_t)irqnum
                }));
}

static irq_index_config_t *pch_cus_claim_irq_index(pch_irq_index_t irq_index) {
        assert(irq_index >= 0 && irq_index < NUM_IRQ_INDEXES);

        irq_index_config_t *ic = get_irq_index_config(irq_index);
        assert(ic->state == IRQIX_UNUSED);

        uint8_t core_num = get_core_num();
        ic->core_num = core_num;
        ic->state = IRQIX_CLAIMED;

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CLAIM_IRQ_INDEX,
                ((struct pch_trdata_id_byte){
                        .id = irq_index,
                        .byte = core_num
                }));

        return ic;
}

static void configure_irq_handler(uint irqnum, irq_handler_t handler, int order_priority) {
        if (order_priority == -1)
                irq_set_exclusive_handler(irqnum, handler);
        else
                irq_add_shared_handler(irqnum, handler, order_priority);

        irq_set_enabled(irqnum, true);
        trace_configure_irq_handler(PCH_TRC_RT_CUS_INIT_IRQ_HANDLER,
                irqnum, handler, (int16_t)order_priority);
}

void pch_cus_configure_dma_irq(pch_irq_index_t irq_index, int order_priority) {
        irq_index_config_t *ic = get_irq_index_config(irq_index);
        assert(ic->state == IRQIX_CLAIMED);
        assert(!ic->dma_irq_configured);
        irq_num_t irqnum = dma_get_irq_num((uint)irq_index);
        configure_irq_handler(irqnum, pch_cus_handle_dma_irq,
                order_priority);
        ic->dma_irq_configured = true;
}

void pch_cus_configure_dma_irq_exclusive(pch_irq_index_t irq_index) {
        pch_cus_configure_dma_irq(irq_index, -1);
}

void pch_cus_configure_dma_irq_shared(pch_irq_index_t irq_index, uint8_t order_priority) {
        pch_cus_configure_dma_irq(irq_index, order_priority);
}

void pch_cus_configure_dma_irq_shared_default(pch_irq_index_t irq_index) {
        pch_cus_configure_dma_irq_shared(irq_index,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
}

void pch_cus_configure_dma_irq_if_unset(pch_irq_index_t irq_index) {
        irq_index_config_t *ic = get_irq_index_config(irq_index);
        if (!ic->dma_irq_configured)
                pch_cus_configure_dma_irq_shared_default(irq_index);
}

void pch_cu_set_irq_index(pch_cu_t *cu, pch_irq_index_t irq_index) {
        assert(irq_index >= 0 && irq_index < NUM_IRQ_INDEXES);
        assert(cu->irq_index == -1 || cu->irq_index == irq_index);
        cu->irq_index = irq_index;
        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_SET_IRQ_INDEX,
                ((struct pch_trdata_id_byte){
                        .id = cu->cuaddr,
                        .byte = irq_index
                }));
}

pch_irq_index_t pch_cus_find_or_claim_irq_index(void) {
        uint core_num = get_core_num();
        int first_unused = -1;
        pch_irq_index_t irq_index;

        for (irq_index = 0; irq_index < NUM_IRQ_INDEXES; irq_index++) {
                irq_index_config_t *ic = get_irq_index_config(irq_index);
                if (ic->state == IRQIX_CLAIMED) {
                        if (ic->core_num == core_num)
                                return irq_index; // found one for our core
                } else if (ic->state == IRQIX_UNUSED) {
                        if (first_unused == -1)
                                first_unused = irq_index;
                }
        }

        // No already-claimed irq_index. If the one corresponding to
        // our core_num is available, use that, otherwise use the
        // lowest-numbered unused one, panicking if there wasn't one.
        if (get_irq_index_config(core_num)->state == IRQIX_UNUSED)
                irq_index = core_num;
        else if (first_unused == -1)
                panic("no available IRQ indexes");
        else
                irq_index = first_unused;

        pch_cus_claim_irq_index(irq_index);
        return irq_index;
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
