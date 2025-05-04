/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"

void process_schib_func(pch_schib_t *schib);
void process_schib_response(css_cu_t *cu, pch_schib_t *schib);

static inline pch_schib_t *pop_ua_func_dlist(css_cu_t *cu) {
        return pop_ua_dlist(&cu->ua_func_dlist, cu);
}

static void css_handle_dma_irq_cu(pch_dma_irq_index_t dmairqix, css_cu_t *cu) {
        dmachan_rx_channel_t *rx = &cu->rx_channel;
        if (dmachan_rx_irq_raised(rx, dmairqix)) {
                dmachan_ack_rx_irq(rx, dmairqix);
		css_handle_rx_complete(cu);
	}

        dmachan_tx_channel_t *tx = &cu->tx_channel;
        if (dmachan_tx_irq_raised(tx, dmairqix)) {
                dmachan_ack_tx_irq(tx, dmairqix);
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
        for (int i = 0; i < PCH_NUM_CSS_CUS; i++) {
		css_cu_t *cu = &CSS.cus[i];
                if (!cu->started)
			continue;

		css_handle_dma_irq_cu(dmairqix, cu);
	}
}
