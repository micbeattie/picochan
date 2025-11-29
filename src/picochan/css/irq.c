/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "css_internal.h"
#include "css_trace.h"

void process_schib_func(pch_schib_t *schib);
void process_schib_response(pch_chp_t *chp, pch_schib_t *schib);

static inline pch_schib_t *pop_ua_func_dlist(pch_chp_t *chp) {
        return pop_ua_dlist(&chp->ua_func_dlist, chp);
}

// process_a_schib_waiting_for_tx return value is progress,
// true when progress has been made and there may be another
// schib waiting for tx
static bool process_a_schib_waiting_for_tx(pch_chp_t *chp) {
        if (pch_chp_is_tx_active(chp))
                return false; // tx busy

        pch_schib_t *schib = pop_ua_response_slist(chp);
        if (schib) {
                process_schib_response(chp, schib);
                return true;
        }

        schib = pop_ua_func_dlist(chp);
        if (schib) {
                process_schib_func(schib);
                return true;
        }

        return false;
}

static void handle_dma_irq_chp(pch_chp_t *chp) {
        dmachan_tx_channel_t *tx = &chp->tx_channel;
        dmachan_irq_state_t tx_irq_state = dmachan_handle_tx_irq(tx);
        dmachan_rx_channel_t *rx = &chp->rx_channel;
        dmachan_irq_state_t rx_irq_state = dmachan_handle_rx_irq(rx);

        trace_chp_irq(PCH_TRC_RT_CSS_CHP_IRQ, chp, CSS.dmairqix,
                tx_irq_state, rx_irq_state);

        dmachan_link_t *txl = &tx->link;
        dmachan_link_t *rxl = &rx->link;
        bool progress = true;

        trace_chp_irq_progress(PCH_TRC_RT_CSS_CHP_IRQ_PROGRESS,
                chp, rxl->complete, txl->complete, progress);
        while (rxl->complete || txl->complete || progress) {
                if (rxl->complete) {
                        rxl->complete = false;
                        css_handle_rx_complete(chp);
                }

                trace_chp_irq_progress(PCH_TRC_RT_CSS_CHP_IRQ_PROGRESS,
                        chp, rxl->complete, txl->complete, progress);
                if (txl->complete) {
                        txl->complete = false;
                        css_handle_tx_complete(chp);
                }

                trace_chp_irq_progress(PCH_TRC_RT_CSS_CHP_IRQ_PROGRESS,
                        chp, rxl->complete, txl->complete, progress);
                progress = process_a_schib_waiting_for_tx(chp);
                trace_chp_irq_progress(PCH_TRC_RT_CSS_CHP_IRQ_PROGRESS,
                        chp, rxl->complete, txl->complete, progress);
        }
}

void __time_critical_func(handle_func_irq_chp)(pch_chp_t *chp) {
        PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_FUNC_IRQ,
                pch_chp_is_traced_irq(chp), ((struct pch_trdata_func_irq){
                .ua_opt = peek_ua_dlist(&chp->ua_func_dlist),
                .chpid = pch_get_chpid(chp),
                .tx_active = (int8_t)pch_chp_is_tx_active(chp)
                }));

	while (!pch_chp_is_tx_active(chp)) {
                pch_schib_t *schib = pop_ua_func_dlist(chp);
		if (!schib)
			break;

		process_schib_func(schib);
	}
}

void __isr __time_critical_func(pch_css_func_irq_handler)(void) {
        uint irqnum = __get_current_exception() - VTABLE_FIRST_IRQ;
        if ((int16_t)irqnum != CSS.func_irqnum)
                return;

	irq_clear(irqnum);

        for (int i = 0; i < PCH_NUM_CHANNELS; i++) {
		pch_chp_t *chp = &CSS.chps[i];
		if (!pch_chp_is_started(chp))
			continue;

		if (pch_chp_is_tx_active(chp))
			continue;

		handle_func_irq_chp(chp);
	}
}

void __isr __time_critical_func(pch_css_dma_irq_handler)() {
        uint irqnum = __get_current_exception() - VTABLE_FIRST_IRQ;
	// TODO deal with getting the Irq information and acking
	// them in a batch instead of individually
        pch_dma_irq_index_t dmairqix = (pch_dma_irq_index_t)(irqnum - DMA_IRQ_0);
        if (dmairqix != CSS.dmairqix)
                return;

        for (int i = 0; i < PCH_NUM_CHANNELS; i++) {
		pch_chp_t *chp = &CSS.chps[i];
                if (!pch_chp_is_started(chp))
			continue;

		handle_dma_irq_chp(chp);
	}
}
