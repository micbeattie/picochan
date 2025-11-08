/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "css_internal.h"
#include "css_trace.h"

dmachan_tx_channel_t *pch_chp_get_tx_channel(pch_chpid_t chpid) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp));

        return &chp->tx_channel;
}

dmachan_rx_channel_t *pch_chp_get_rx_channel(pch_chpid_t chpid) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp));

        return &chp->rx_channel;
}

void pch_chp_claim(pch_chpid_t chpid) {
        pch_chp_t *chp = pch_get_chp(chpid);
        if (pch_chp_is_allocated(chp))
                panic("channel path already allocated");

        if (pch_chp_is_claimed(chp))
                panic("channel path already claimed");

        pch_chp_set_claimed(chp, true);
}

int pch_chp_claim_unused(bool required) {
        for (int i = 0; i < PCH_NUM_CHANNELS; i++) {
                pch_chp_t *chp = pch_get_chp(i);
                if (!pch_chp_is_claimed(chp) && !pch_chp_is_allocated(chp)) {
                        pch_chp_set_claimed(chp, true);
                        return i;
                }
        }

        if (required)
                panic("No channel paths are available");

        return -1;
}

pch_sid_t pch_chp_alloc(pch_chpid_t chpid, uint16_t num_devices) {
        assert(css_is_started());
	pch_chp_t *chp = pch_get_chp(chpid);
        assert(!pch_chp_is_allocated(chp));

	pch_sid_t first_sid = CSS.next_sid;
	valid_params_if(PCH_CSS,
                first_sid < PCH_NUM_SCHIBS);
        valid_params_if(PCH_CSS,
                num_devices >= 1 && num_devices <= 256);
        valid_params_if(PCH_CSS,
                (int)first_sid+(int)num_devices <= PCH_NUM_SCHIBS);

	CSS.next_sid += (pch_sid_t)num_devices;

        memset(chp, 0, sizeof *chp);
	chp->first_sid = first_sid;
	chp->num_devices = num_devices;
	chp->rx_data_for_ua = -1;
	chp->ua_func_dlist = -1;
        chp->ua_response_slist.head = -1;
        chp->ua_response_slist.tail = -1;
        pch_chp_set_allocated(chp, true);

	for (int i = 0; i < num_devices; i++) {
		pch_unit_addr_t ua = (pch_unit_addr_t)i;
		pch_sid_t sid = first_sid + (pch_sid_t)i;
		pch_schib_t *schib = get_schib(sid);
		schib->pmcw.chpid = chpid;
		schib->pmcw.unit_addr = ua;
	}

        PCH_CSS_TRACE(PCH_TRC_RT_CSS_CHP_ALLOC,
                ((struct pch_trdata_chp_alloc){
                        .first_sid = first_sid,
                        .num_devices = num_devices,
                        .chpid = chpid
                }));

        return first_sid;
}

static inline void trace_chp_dma(pch_trc_record_type_t rt, pch_chpid_t chpid, dmachan_1way_config_t *d1c) {
        PCH_CSS_TRACE(rt, ((struct pch_trdata_dma_init){
                .addr = d1c->addr,
                .ctrl = channel_config_get_ctrl_value(&d1c->ctrl),
                .id = chpid,
                .dmaid = d1c->dmaid,
                .dmairqix = d1c->dmairqix,
                .core_num = (uint8_t)get_core_num()
        }));
}

static void chp_dma_tx_init(pch_chpid_t chpid, dmachan_1way_config_t *d1c) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp) && !pch_chp_is_started(chp));

        dmachan_init_tx_channel(&chp->tx_channel, d1c);
        trace_chp_dma(PCH_TRC_RT_CSS_CHP_TX_DMA_INIT, chpid, d1c);
}

static void chp_dma_rx_init(pch_chpid_t chpid, dmachan_1way_config_t *d1c) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp) && !pch_chp_is_started(chp));

        dmachan_init_rx_channel(&chp->rx_channel, d1c);
        trace_chp_dma(PCH_TRC_RT_CSS_CHP_RX_DMA_INIT, chpid, d1c);
}

void pch_chp_dma_configure(pch_chpid_t chpid, dmachan_config_t *dc) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp) && !pch_chp_is_started(chp));
        (void)chp;

        chp_dma_tx_init(chpid, &dc->tx);
        chp_dma_rx_init(chpid, &dc->rx);
}

void pch_chp_mark_configure_complete(pch_chpid_t chpid, bool configured) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp));

        pch_chp_set_configured(chp, configured);

        PCH_CSS_TRACE(PCH_TRC_RT_CSS_CHP_CONFIGURED,
                ((struct pch_trdata_id_byte){
                        .id = chpid,
                        .byte = (uint8_t)configured
                }));
}

void pch_chp_configure_uartchan(pch_chpid_t chpid, uart_inst_t *uart, dma_channel_config ctrl) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp) && !pch_chp_is_started(chp));

        dma_channel_config txctrl = dmachan_uart_make_txctrl(uart, ctrl);
        dma_channel_config rxctrl = dmachan_uart_make_rxctrl(uart, ctrl);
        uint32_t hwaddr = (uint32_t)&uart_get_hw(uart)->dr; // read/write fifo
        dmachan_config_t dc = dmachan_config_claim(hwaddr, txctrl,
                hwaddr, rxctrl, CSS.dmairqix);
        pch_chp_dma_configure(chpid, &dc);
        dmachan_set_link_irq_enabled(&chp->tx_channel.link, true);
        dmachan_set_link_irq_enabled(&chp->rx_channel.link, true);
        pch_chp_mark_configure_complete(chpid, true);
}

void pch_chp_auto_configure_uartchan(pch_chpid_t chpid, uart_inst_t *uart, uint baudrate) {
        pch_uart_init(uart, baudrate);

        // Argument 0 is ok here (as would be any DMA id) because it
        // only affects the "chain-to" value and that is overridden in
        // pch_chp_configure_uartchan() anyway.
        dma_channel_config ctrl = dma_channel_get_default_config(0);
        pch_chp_configure_uartchan(chpid, uart, ctrl);
}

void pch_chp_configure_memchan(pch_chpid_t chpid, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer) {
        // Check that spin_lock is initialised even when not a Debug
        // release because silently ignoring it produces such
        // nasty-to-troubleshoot race conditions
        dmachan_panic_unless_memchan_initialised();

        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp) && !pch_chp_is_started(chp));

        dmachan_config_t dc = dmachan_config_memchan_make(txdmaid,
                rxdmaid, CSS.dmairqix);
        pch_chp_dma_configure(chpid, &dc);
        // Do not enable irq for tx channel link because Pico DMA
        // does not treat the INTSn bits separately. We enable only
        // the rx side for irqs and the rx irq handler propagates
        // notifications to the tx side via the INTFn "forced irq"
        // register which overrides the INTEn enabled bits.
        dmachan_rx_channel_t *rx = &chp->rx_channel;
        dmachan_set_link_irq_enabled(&rx->link, true);
        txpeer->mem_rx_peer = rx;
        rx->mem_tx_peer = txpeer;
        pch_chp_mark_configure_complete(chpid, true);
}

bool pch_chp_set_trace(pch_chpid_t chpid, bool trace) {
	pch_chp_t *chp = pch_get_chp(chpid);
	bool old_trace = pch_chp_is_traced(chp);
	pch_chp_set_traced(chp, trace);
        if (trace) {
                chp->tx_channel.link.bs = &CSS.trace_bs;
                chp->rx_channel.link.bs = &CSS.trace_bs;
        } else {
                chp->tx_channel.link.bs = 0;
                chp->rx_channel.link.bs = 0;
        }

	PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_CHP_TRACED,
                trace || old_trace,
		((struct pch_trdata_id_byte){
                        .id = chpid,
                        .byte = (uint8_t)trace
        }));

	return old_trace;
}

void pch_chp_start(pch_chpid_t chpid) {
	pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_configured(chp));

        if (pch_chp_is_started(chp))
                return;

	PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_CHP_STARTED,
                pch_chp_is_traced(chp),
		((struct pch_trdata_id_byte){
                        .id = chpid,
                        .byte = 1
        }));

        pch_chp_set_started(chp, true);
        dmachan_start_dst_cmdbuf(&chp->rx_channel);
        dmachan_write_src_reset(&chp->tx_channel);
}
