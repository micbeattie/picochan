/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "css_trace.h"

// CSS is a channel subsystem. It is intended to be a singleton and
// is just a convenience for gathering together the global variables
// associated with the CSS.
struct css __not_in_flash("picochan_css") CSS;

unsigned char pch_css_trace_buffer_space[PCH_TRC_NUM_BUFFERS * PCH_TRC_BUFFER_SIZE] __aligned(4);

static inline bool css_is_started(void) {
        return CSS.dmairqix >= 0;
}

static inline uint8_t css_dmairqix(void) {
        assert(css_is_started());
        return (uint8_t)CSS.dmairqix;
}

void pch_css_init(void) {
        memset(&CSS, 0, sizeof CSS);

        pch_trc_init_bufferset(&CSS.trace_bs,
                PCH_CSS_BUFFERSET_MAGIC);
        pch_trc_init_all_buffers(&CSS.trace_bs,
                pch_css_trace_buffer_space);

        for (int i = 0; i < PCH_NUM_ISCS; i++)
                CSS.isc_dlists[i] = -1;

        CSS.func_irqnum = -1;
        CSS.io_irqnum = -1;
        CSS.dmairqix = -1; // CSS not yet started

        for (int i = 0; i < PCH_NUM_SCHIBS; i++) {
                pch_schib_t *schib = &CSS.schibs[i];
                // point nextsid at self to indicate end of list
                schib->mda.nextsid = (pch_sid_t)i;
        }
}

void pch_css_start(uint8_t dmairqix) {
        valid_params_if(PCH_CSS, dmairqix <= 127);
        irq_num_t irqnum = dma_get_irq_num(dmairqix);
        irq_add_shared_handler(irqnum, css_handle_dma_irq,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(irqnum, true);
        PCH_CSS_TRACE(PCH_TRC_RT_CSS_INIT_DMA_IRQ_HANDLER,
                ((struct pch_trc_trdata_word_byte){
                        .word = (uint32_t)css_handle_dma_irq,
                        .byte = (uint8_t)irqnum
                }));

        CSS.dmairqix = (int8_t)dmairqix; // non-negative from assert
}

static void init_cu(css_cu_t *cu, pch_cunum_t cunum, pch_sid_t first_sid, uint16_t num_devices) {
	valid_params_if(PCH_CSS,
                first_sid < PCH_NUM_SCHIBS);
        valid_params_if(PCH_CSS,
                num_devices >= 1 && num_devices <= 256);
        valid_params_if(PCH_CSS,
                (int)first_sid+(int)num_devices <= PCH_NUM_SCHIBS);

        memset(cu, 0, sizeof *cu);
	cu->cunum = cunum;
	cu->first_sid = first_sid;
	cu->num_devices = num_devices;
	cu->rx_data_for_ua = -1;
	cu->ua_func_dlist = -1;
        cu->ua_response_slist.head = -1;
        cu->ua_response_slist.tail = -1;

	for (int i = 0; i < num_devices; i++) {
		pch_unit_addr_t ua = (pch_unit_addr_t)i;
		pch_sid_t sid = first_sid + (pch_sid_t)i;
		pch_schib_t *schib = get_schib(sid);
		schib->pmcw.cu_number = cunum;
		schib->pmcw.unit_addr = ua;
	}

	cu->enabled = true;
}

void pch_css_register_cu(pch_cunum_t cunum, uint16_t num_devices, uint32_t txhwaddr, dma_channel_config txctrl, uint32_t rxhwaddr, dma_channel_config rxctrl) {
        assert(css_is_started());
	css_cu_t *cu = get_cu(cunum);
	pch_sid_t first_sid = CSS.next_sid;
	init_cu(cu, cunum, first_sid, num_devices);
	CSS.next_sid += (pch_sid_t)num_devices;

	pch_dmaid_t txdmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        dmachan_init_tx_channel(&cu->tx_channel, txdmaid, txhwaddr, txctrl);
        dma_irqn_set_channel_enabled(css_dmairqix(), txdmaid, true);

	pch_dmaid_t rxdmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        dmachan_init_rx_channel(&cu->rx_channel, rxdmaid, rxhwaddr, rxctrl);
        dma_irqn_set_channel_enabled(css_dmairqix(), rxdmaid, true);

        PCH_CSS_TRACE(PCH_TRC_RT_CSS_REGISTER_CU,
                ((struct trdata_register_cu){cunum, first_sid, num_devices}));
}

void pch_css_register_mem_cu(pch_cunum_t cunum, uint16_t num_devices, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid) {
        assert(css_is_started());
	css_cu_t *cu = get_cu(cunum);
	pch_sid_t first_sid = CSS.next_sid;
	init_cu(cu, cunum, first_sid, num_devices);
	CSS.next_sid += (pch_sid_t)num_devices;

        dma_channel_config czero = {0};
        dmachan_init_tx_channel(&cu->tx_channel, txdmaid, 0, czero);
        dmachan_init_rx_channel(&cu->rx_channel, rxdmaid, 0, czero);

        dma_irqn_set_channel_enabled(css_dmairqix(), txdmaid, true);
        dma_irqn_set_channel_enabled(css_dmairqix(), rxdmaid, true);

        PCH_CSS_TRACE(PCH_TRC_RT_CSS_REGISTER_MEM_CU,
		((struct trdata_register_mem_cu){cunum, first_sid, num_devices, txdmaid, rxdmaid}));
}

bool pch_css_set_trace_cu(pch_cunum_t cunum, bool trace) {
	css_cu_t *cu = get_cu(cunum);
	bool old_trace = cu->trace;
	cu->trace = trace;
	PCH_TRC_WRITE(&CSS.trace_bs, trace || old_trace,
		PCH_TRC_RT_CSS_SET_TRACE_CU,
		((struct trdata_cunum_traceold_tracenew){cunum, old_trace, trace}));

	return old_trace;
}

void pch_css_start_channel(pch_cunum_t cunum) {
	css_cu_t *cu = get_cu(cunum);
        dmachan_start_dst_cmdbuf(&cu->rx_channel);
}

void pch_css_set_func_irq(irq_num_t irqnum) {
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_FUNC_IRQ,
                ((struct trdata_irqnum_opt){irqnum}));
        irq_set_enabled(irqnum, true);
	CSS.func_irqnum = (int16_t)irqnum;
}

void pch_css_set_io_irq(irq_num_t irqnum) {
        user_irq_claim(irqnum);
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_IO_IRQ,
                ((struct trdata_irqnum_opt){irqnum}));
	CSS.io_irqnum = (int16_t)irqnum;
}

void pch_css_unset_io_irq(void) {
        int16_t io_irqnum_opt = CSS.io_irqnum;
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_IO_IRQ,
                ((struct trdata_irqnum_opt){-1}));
        if (io_irqnum_opt < 0)
                return;

        irq_num_t irqnum = (irq_num_t)io_irqnum_opt;

	CSS.io_irqnum = -1;
        user_irq_unclaim(irqnum);
}

io_callback_t pch_css_set_io_callback(io_callback_t io_callback) {
	io_callback_t old_io_callback = CSS.io_callback;
        PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_IO_CALLBACK,
                ((struct trdata_address_change){
                        (uint32_t)old_io_callback,
                        (uint32_t)io_callback
                }));
	CSS.io_callback = io_callback;
	return old_io_callback;
}

bool pch_css_set_trace(bool trace) {
        return pch_trc_set_enable(&CSS.trace_bs, trace);
}

void __time_critical_func(send_tx_packet)(css_cu_t *cu, proto_packet_t p) {
        memcpy(cu->tx_channel.cmdbuf, &p, sizeof p);
        cu->tx_active = true;
        dmachan_start_src_cmdbuf(&cu->tx_channel);
}
