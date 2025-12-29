/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "css_internal.h"
#include "css_trace.h"

pch_channel_t *pch_chp_get_channel(pch_chpid_t chpid) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp));

        return &chp->channel;
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

static inline void trace_chp_dma(pch_trc_record_type_t rt, pch_chpid_t chpid, dmachan_link_t *l) {
        PCH_CSS_TRACE(rt, ((struct pch_trdata_dma_init){
                .ctrl = dma_get_ctrl_value(l->dmaid),
                .id = chpid,
                .dmaid = l->dmaid,
                .dmairqix = l->irq_index,
                .core_num = (uint8_t)get_core_num()
        }));
}

void pch_chp_configure_uartchan(pch_chpid_t chpid, uart_inst_t *uart, pch_uartchan_config_t *cfg) {
        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp));

        pch_channel_init_uartchan(&chp->channel, chpid, uart, cfg);

        trace_chp_dma(PCH_TRC_RT_CSS_CHP_TX_DMA_INIT, chpid,
                &chp->channel.tx.link);
        trace_chp_dma(PCH_TRC_RT_CSS_CHP_RX_DMA_INIT, chpid,
                &chp->channel.rx.link);
}

void pch_chp_configure_memchan(pch_chpid_t chpid, pch_channel_t *chpeer) {
        // Check that spin_lock is initialised even when not a Debug
        // release because silently ignoring it produces such
        // nasty-to-troubleshoot race conditions
        dmachan_panic_unless_memchan_initialised();

        pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_chp_is_allocated(chp));

        pch_channel_init_memchan(&chp->channel, chpid, CSS.dmairqix, chpeer);

        trace_chp_dma(PCH_TRC_RT_CSS_CHP_TX_DMA_INIT, chpid,
                &chp->channel.tx.link);
        trace_chp_dma(PCH_TRC_RT_CSS_CHP_RX_DMA_INIT, chpid,
                &chp->channel.rx.link);
}

static void set_dmachan_links_bs(pch_chp_t *chp, pch_trc_bufferset_t *bs) {
        dmachan_set_link_bs(&chp->channel.tx.link, bs);
        dmachan_set_link_bs(&chp->channel.rx.link, bs);
}

uint8_t pch_chp_set_trace_flags(pch_chpid_t chpid, uint8_t trace_flags) {
	pch_chp_t *chp = pch_get_chp(chpid);
        trace_flags &= PCH_CHP_TRACED_MASK;
        uint8_t old_trace_flags = chp->trace_flags;
        chp->trace_flags = trace_flags;

        if (trace_flags & PCH_CHP_TRACED_LINK)
                set_dmachan_links_bs(chp, &CSS.trace_bs);
        else
                set_dmachan_links_bs(chp, NULL);

	PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_CHP_TRACED,
                trace_flags != old_trace_flags,
		((struct pch_trdata_id_byte){
                        .id = chpid,
                        .byte = trace_flags
        }));

	return old_trace_flags;
}

bool pch_chp_set_trace(pch_chpid_t chpid, bool trace) {
        uint8_t new_trace_flags = trace ? PCH_CHP_TRACED_MASK : 0;
        uint8_t old_trace_flags =  pch_chp_set_trace_flags(chpid, new_trace_flags);
        return old_trace_flags != new_trace_flags;
}

void pch_chp_start(pch_chpid_t chpid) {
	pch_chp_t *chp = pch_get_chp(chpid);
        assert(pch_channel_is_configured(&chp->channel));

        if (pch_channel_is_started(&chp->channel))
                return;

	PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_CHP_STARTED,
                pch_chp_is_traced_general(chp),
		((struct pch_trdata_id_byte){
                        .id = chpid,
                        .byte = 1
        }));

        pch_channel_set_started(&chp->channel, true);
        dmachan_start_dst_cmdbuf(&chp->channel.rx);
        dmachan_write_src_reset(&chp->channel.tx);
}
