/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "css_trace.h"

void process_schib_func(pch_schib_t *schib);
void process_schib_response(css_cu_t *cu, pch_schib_t *schib);

static inline pch_schib_t *pop_ua_func_dlist(css_cu_t *cu) {
        return pop_ua_dlist(&cu->ua_func_dlist, cu);
}

static void css_handle_dma_irq_cu(css_cu_t *cu) {
        dmachan_tx_channel_t *tx = &cu->tx_channel;
        bool tx_irq_raised = dmachan_tx_irq_raised(tx);

        dmachan_rx_channel_t *rx = &cu->rx_channel;
        bool rx_irq_raised = dmachan_rx_irq_raised(rx);

        trace_css_cu_irq(PCH_TRC_RT_CSS_CU_IRQ, cu, CSS.dmairqix,
                tx_irq_raised, rx_irq_raised);

        if (rx_irq_raised) {
                dmachan_ack_rx_irq(rx);
		css_handle_rx_complete(cu);
	}

        if (tx_irq_raised) {
                dmachan_ack_tx_irq(tx);
		css_handle_tx_complete(cu);
	}

	// While/if tx dma engine is still free, we can send some
	// responses or user-initiated Start/Clear/Halt commands if
	// there any pending.
	while (!cu->tx_active) {
                pch_schib_t *schib = pop_ua_response_slist(cu);
                if (!schib)
			break;

                process_schib_response(cu, schib);
	}
}

void __time_critical_func(handle_func_irq_cu)(css_cu_t *cu) {
        PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_FUNC_IRQ,
                cu->traced, ((struct trdata_func_irq){
                .ua_opt = peek_ua_dlist(&cu->ua_func_dlist),
                .cunum = cu->cunum,
                .tx_active = (int8_t)cu->tx_active
                }));

	while (!cu->tx_active) {
                pch_schib_t *schib = pop_ua_func_dlist(cu);
		if (!schib)
			break;

		process_schib_func(schib);
	}
}

void __isr __time_critical_func(pch_css_schib_func_irq_handler)(void) {
        uint irqnum = __get_current_exception() - VTABLE_FIRST_IRQ;
        if ((int16_t)irqnum != CSS.func_irqnum)
                return;

	irq_clear(irqnum);

        for (int i = 0; i < PCH_NUM_CSS_CUS; i++) {
		css_cu_t *cu = &CSS.cus[i];
		if (!cu->started)
			continue;

		if (cu->tx_active)
			continue;

		handle_func_irq_cu(cu);
	}
}

void __isr __time_critical_func(css_handle_dma_irq)() {
        uint irqnum = __get_current_exception() - VTABLE_FIRST_IRQ;
	// TODO deal with getting the Irq information and acking
	// them in a batch instead of individually
        pch_dma_irq_index_t dmairqix = (pch_dma_irq_index_t)(irqnum - DMA_IRQ_0);
        if (dmairqix != CSS.dmairqix)
                return;

        for (int i = 0; i < PCH_NUM_CSS_CUS; i++) {
		css_cu_t *cu = &CSS.cus[i];
                if (!cu->started)
			continue;

		css_handle_dma_irq_cu(cu);
	}
}
