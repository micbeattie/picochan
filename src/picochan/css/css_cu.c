/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "css_trace.h"

dmachan_tx_channel_t *pch_css_cu_get_tx_channel(pch_cunum_t cunum) {
        css_cu_t *cu = get_cu(cunum);
        assert(cu->claimed);

        return &cu->tx_channel;
}

dmachan_rx_channel_t *pch_css_cu_get_rx_channel(pch_cunum_t cunum) {
        css_cu_t *cu = get_cu(cunum);
        assert(cu->claimed);

        return &cu->rx_channel;
}

static css_cu_t *css_cu_claim(pch_cunum_t cunum, uint16_t num_devices) {
        assert(css_is_started());
	css_cu_t *cu = get_cu(cunum);
        assert(!cu->claimed);

	pch_sid_t first_sid = CSS.next_sid;
	valid_params_if(PCH_CSS,
                first_sid < PCH_NUM_SCHIBS);
        valid_params_if(PCH_CSS,
                num_devices >= 1 && num_devices <= 256);
        valid_params_if(PCH_CSS,
                (int)first_sid+(int)num_devices <= PCH_NUM_SCHIBS);

	CSS.next_sid += (pch_sid_t)num_devices;

        memset(cu, 0, sizeof *cu);
	cu->cunum = cunum;
	cu->first_sid = first_sid;
	cu->num_devices = num_devices;
	cu->rx_data_for_ua = -1;
	cu->ua_func_dlist = -1;
        cu->ua_response_slist.head = -1;
        cu->ua_response_slist.tail = -1;
        cu->claimed = true;

	for (int i = 0; i < num_devices; i++) {
		pch_unit_addr_t ua = (pch_unit_addr_t)i;
		pch_sid_t sid = first_sid + (pch_sid_t)i;
		pch_schib_t *schib = get_schib(sid);
		schib->pmcw.cu_number = cunum;
		schib->pmcw.unit_addr = ua;
	}

        PCH_CSS_TRACE(PCH_TRC_RT_CSS_CU_CLAIM,
                ((struct trdata_css_cu_claim){
                        .first_sid = first_sid,
                        .num_devices = num_devices,
                        .cunum = cunum
                }));

        return cu;
}

void pch_css_cu_claim(pch_cunum_t cunum, uint16_t num_devices) {
        // Same as css_cu_claim but drop the return value since
        // the public API only uses cunum not the css_cu_t*.
        css_cu_claim(cunum, num_devices);
}

static inline void trace_cu_dma(pch_trc_record_type_t rt, pch_cunum_t cunum, dmachan_1way_config_t *d1c) {
        PCH_CSS_TRACE(rt, ((struct pch_trc_trdata_cu_dma){
                .addr = d1c->addr,
                .ctrl = channel_config_get_ctrl_value(&d1c->ctrl),
                .cunum = cunum,
                .dmaid = d1c->dmaid,
                .dmairqix = d1c->dmairqix
        }));
}

static void css_cu_dma_tx_init(pch_cunum_t cunum, dmachan_1way_config_t *d1c) {
        css_cu_t *cu = get_cu(cunum);
        assert(cu->claimed && !cu->started);

        dmachan_init_tx_channel(&cu->tx_channel, d1c);
        trace_cu_dma(PCH_TRC_RT_CSS_CU_TX_DMA_INIT, cunum, d1c);
}

static void css_cu_dma_rx_init(pch_cunum_t cunum, dmachan_1way_config_t *d1c) {
        css_cu_t *cu = get_cu(cunum);
        assert(cu->claimed && !cu->started);

        dmachan_init_rx_channel(&cu->rx_channel, d1c);
        trace_cu_dma(PCH_TRC_RT_CSS_CU_RX_DMA_INIT, cunum, d1c);
}

void pch_css_cu_dma_configure(pch_cunum_t cunum, dmachan_config_t *dc) {
        css_cu_t *cu = get_cu(cunum);
        assert(cu->claimed && !cu->started);

        css_cu_dma_tx_init(cunum, &dc->tx);
        css_cu_dma_rx_init(cunum, &dc->rx);
}

void pch_css_cu_set_configured(pch_cunum_t cunum, bool configured) {
        css_cu_t *cu = get_cu(cunum);
        assert(cu->claimed);

        cu->configured = configured;

        PCH_CSS_TRACE(PCH_TRC_RT_CSS_CU_CONFIGURED,
                ((struct pch_trc_trdata_cu_byte){
                        .cunum = cunum,
                        .byte = (uint8_t)configured
                }));
}

void pch_css_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl) {
        css_cu_t *cu = get_cu(cunum);
        assert(cu->claimed && !cu->started);

        pch_init_uart(uart);

        dma_channel_config txctrl = dmachan_uartcu_make_txctrl(uart, ctrl);
        dma_channel_config rxctrl = dmachan_uartcu_make_rxctrl(uart, ctrl);
        uint32_t hwaddr = (uint32_t)&uart_get_hw(uart)->dr; // read/write fifo
        dmachan_config_t dc = dmachan_config_claim(hwaddr, txctrl,
                hwaddr, rxctrl, CSS.dmairqix);
        pch_css_cu_dma_configure(cunum, &dc);
        dmachan_set_link_irq_enabled(&cu->tx_channel.link, true);
        dmachan_set_link_irq_enabled(&cu->rx_channel.link, true);
        pch_css_cu_set_configured(cunum, true);
}

void pch_css_memcu_configure(pch_cunum_t cunum, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer) {
        // Check that spin_lock is initialised even when not a Debug
        // release because silently ignoring it produces such
        // nasty-to-troubleshoot race conditions
        dmachan_panic_unless_memchan_initialised();

        css_cu_t *cu = get_cu(cunum);
        assert(cu->claimed && !cu->started);

        dmachan_config_t dc = dmachan_config_memchan_make(txdmaid,
                rxdmaid, CSS.dmairqix);
        pch_css_cu_dma_configure(cunum, &dc);
        // Do not enable irq for tx channel link because Pico DMA
        // does not treat the INTSn bits separately. We enable only
        // the rx side for irqs and the rx irq handler propagates
        // notifications to the tx side via the INTFn "forced irq"
        // register which overrides the INTEn enabled bits.
        dmachan_rx_channel_t *rx = &cu->rx_channel;
        dmachan_set_link_irq_enabled(&rx->link, true);
        txpeer->mem_rx_peer = rx;
        rx->mem_tx_peer = txpeer;
        pch_css_cu_set_configured(cunum, true);
}

bool pch_css_set_trace_cu(pch_cunum_t cunum, bool trace) {
	css_cu_t *cu = get_cu(cunum);
	bool old_trace = cu->traced;
	cu->traced = trace;
        if (trace) {
                cu->tx_channel.link.bs = &CSS.trace_bs;
                cu->rx_channel.link.bs = &CSS.trace_bs;
        } else {
                cu->tx_channel.link.bs = 0;
                cu->rx_channel.link.bs = 0;
        }

	PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_CU_TRACED,
                trace || old_trace,
		((struct pch_trc_trdata_cu_byte){
                        .cunum = cunum,
                        .byte = (uint8_t)trace
        }));

	return old_trace;
}

void pch_css_cu_start(pch_cunum_t cunum) {
	css_cu_t *cu = get_cu(cunum);
        assert(cu->configured);

	PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_CU_STARTED,
                cu->traced,
		((struct pch_trc_trdata_cu_byte){
                        .cunum = cunum,
                        .byte = 1
        }));

        cu->started = true;
        dmachan_start_dst_cmdbuf(&cu->rx_channel);
}
