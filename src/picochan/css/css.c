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

// CSS interrupts and callbacks will be handled on the core that calls
// this function
void pch_css_start(uint8_t dmairqix) {
        valid_params_if(PCH_CSS, dmairqix <= 127);
        irq_num_t irqnum = dma_get_irq_num(dmairqix);
        irq_add_shared_handler(irqnum, css_handle_dma_irq,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(irqnum, true);
        PCH_CSS_TRACE(PCH_TRC_RT_CSS_INIT_DMA_IRQ_HANDLER,
                ((struct pch_trdata_word_byte){
                        .word = (uint32_t)css_handle_dma_irq,
                        .byte = (uint8_t)irqnum
                }));

        CSS.dmairqix = (int8_t)dmairqix; // non-negative from assert
}

void pch_css_set_func_irq(irq_num_t irqnum) {
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_FUNC_IRQ,
                ((struct pch_trdata_irqnum_opt){irqnum}));
        irq_set_enabled(irqnum, true);
	CSS.func_irqnum = (int16_t)irqnum;
}

void pch_css_set_io_irq(irq_num_t irqnum) {
        user_irq_claim(irqnum);
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_IO_IRQ,
                ((struct pch_trdata_irqnum_opt){irqnum}));
	CSS.io_irqnum = (int16_t)irqnum;
}

void pch_css_unset_io_irq(void) {
        int16_t io_irqnum_opt = CSS.io_irqnum;
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_IO_IRQ,
                ((struct pch_trdata_irqnum_opt){-1}));
        if (io_irqnum_opt < 0)
                return;

        irq_num_t irqnum = (irq_num_t)io_irqnum_opt;

	CSS.io_irqnum = -1;
        user_irq_unclaim(irqnum);
}

io_callback_t pch_css_set_io_callback(io_callback_t io_callback) {
	io_callback_t old_io_callback = CSS.io_callback;
        PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_IO_CALLBACK,
                ((struct pch_trdata_address_change){
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
        DMACHAN_LINK_CMD_COPY(&cu->tx_channel.link, &p);
        cu->tx_active = true;
        dmachan_start_src_cmdbuf(&cu->tx_channel);
}
