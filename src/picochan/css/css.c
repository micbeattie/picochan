/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "css_trace.h"

//! CSS is a channel subsystem. It is intended to be a singleton and
//! is just a convenience for gathering together the global variables
//! associated with the CSS.
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
        CSS.core_num = -1; // No core_num-dependent IRQs set yet

        for (int i = 0; i < PCH_NUM_SCHIBS; i++) {
                pch_schib_t *schib = &CSS.schibs[i];
                // point nextsid at self to indicate end of list
                schib->mda.nextsid = (pch_sid_t)i;
        }
}

// Helpers for setting CSS IRQ handlers

static void css_try_set_core_num() {
        uint core_num = get_core_num();

        if (CSS.core_num == -1)
                CSS.core_num = (int8_t)core_num;
        else if ((uint)CSS.core_num != core_num)
                panic("CSS configured from multiple cores");

	PCH_CSS_TRACE(PCH_TRC_RT_CSS_CORE_NUM,
                ((struct pch_trdata_byte){(uint8_t)core_num}));
}

static void css_irq_set_exclusive_handler(pch_trc_record_type_t rt, irq_num_t irqnum, irq_handler_t handler) {
        css_try_set_core_num();
        irq_set_exclusive_handler(irqnum, handler);

        PCH_CSS_TRACE(rt, ((struct pch_trdata_irq_handler){
                        .handler = (uint32_t)handler,
                        .order_priority = -1,
                        .irqnum = (uint8_t)irqnum
                }));
}

static void css_irq_add_shared_handler(pch_trc_record_type_t rt, irq_num_t irqnum, irq_handler_t handler, uint8_t order_priority) {
        css_try_set_core_num();
        irq_add_shared_handler(irqnum, handler, order_priority);

        PCH_CSS_TRACE(rt, ((struct pch_trdata_irq_handler){
                        .handler = (uint32_t)handler,
                        .order_priority = (int16_t)order_priority,
                        .irqnum = (uint8_t)irqnum
                }));
}

int8_t pch_css_get_core_num(void) {
        return CSS.core_num;
}

// Configuring DMA IRQ handler

pch_dma_irq_index_t pch_css_get_dma_irq_index(void) {
        return CSS.dmairqix;
}

void pch_css_set_dma_irq_index(pch_dma_irq_index_t dmairqix) {
        if (dmairqix < 0 || dmairqix >= NUM_DMA_IRQS)
                panic("invalid DMA IRQ index");

        irq_num_t irqnum = dma_get_irq_num((uint)dmairqix);
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_DMA_IRQ,
                ((struct pch_trdata_irqnum_opt){irqnum}));

        CSS.dmairqix = dmairqix;
}

void pch_css_configure_dma_irq_index_shared(pch_dma_irq_index_t dmairqix, uint8_t order_priority)
 {
        pch_css_set_dma_irq_index(dmairqix);
        irq_num_t irqnum = dma_get_irq_num(dmairqix);
        css_irq_add_shared_handler(PCH_TRC_RT_CSS_INIT_DMA_IRQ_HANDLER,
                irqnum, pch_css_dma_irq_handler, order_priority);
        irq_set_enabled(irqnum, true);
}

void pch_css_configure_dma_irq_index_exclusive(pch_dma_irq_index_t dmairqix) {
        pch_css_set_dma_irq_index(dmairqix);
        irq_num_t irqnum = dma_get_irq_num(dmairqix);
        css_irq_set_exclusive_handler(PCH_TRC_RT_CSS_INIT_DMA_IRQ_HANDLER,
                irqnum, pch_css_dma_irq_handler);
        irq_set_enabled(irqnum, true);
}

void pch_css_configure_dma_irq_index_default_shared(uint8_t order_priority) {
        pch_dma_irq_index_t dmairqix = (pch_dma_irq_index_t)get_core_num();
        pch_css_configure_dma_irq_index_shared(dmairqix, order_priority);
}

void pch_css_configure_dma_irq_index_default_exclusive() {
        pch_dma_irq_index_t dmairqix = (pch_dma_irq_index_t)get_core_num();
        pch_css_configure_dma_irq_index_exclusive(dmairqix);
}

void pch_css_auto_configure_dma_irq_index() {
        pch_css_configure_dma_irq_index_default_shared(PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
}

// Configuring function IRQ handler

int16_t pch_css_get_func_irq(void) {
        return CSS.func_irqnum;
}

void pch_css_set_func_irq(irq_num_t irqnum) {
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_FUNC_IRQ,
                ((struct pch_trdata_irqnum_opt){irqnum}));
	CSS.func_irqnum = (int16_t)irqnum;
}

void pch_css_configure_func_irq_shared(irq_num_t irqnum, uint8_t order_priority) {
        css_irq_add_shared_handler(PCH_TRC_RT_CSS_INIT_FUNC_IRQ_HANDLER,
                irqnum, pch_css_func_irq_handler, order_priority);
        pch_css_set_func_irq(irqnum);
        irq_set_enabled(irqnum, true);
}

void pch_css_configure_func_irq_exclusive(irq_num_t irqnum) {
        css_irq_set_exclusive_handler(PCH_TRC_RT_CSS_INIT_FUNC_IRQ_HANDLER,
                irqnum, pch_css_func_irq_handler);
        pch_css_set_func_irq(irqnum);
        irq_set_enabled(irqnum, true);
}

void pch_css_configure_func_irq_unused_shared(bool required, uint8_t order_priority) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_func_irq_shared(irqnum, order_priority);
        }
}

void pch_css_configure_func_irq_unused_exclusive(bool required) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_func_irq_exclusive(irqnum);
        }
}

void pch_css_auto_configure_func_irq(bool required) {
        pch_css_configure_func_irq_unused_shared(required,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
}

// Configuring I/O IRQ handler

int16_t pch_css_get_io_irq(void) {
        return CSS.io_irqnum;
}

void pch_css_set_io_irq(irq_num_t irqnum) {
	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_IO_IRQ,
                ((struct pch_trdata_irqnum_opt){irqnum}));
	CSS.io_irqnum = (int16_t)irqnum;
}

void pch_css_configure_io_irq_shared(irq_num_t irqnum, uint8_t order_priority) {
        css_irq_add_shared_handler(PCH_TRC_RT_CSS_INIT_IO_IRQ_HANDLER,
                irqnum, pch_css_io_irq_handler, order_priority);
        pch_css_set_io_irq(irqnum);
        irq_set_enabled(irqnum, true);
}

void pch_css_configure_io_irq_exclusive(irq_num_t irqnum) {
        css_irq_set_exclusive_handler(PCH_TRC_RT_CSS_INIT_IO_IRQ_HANDLER,
                irqnum, pch_css_io_irq_handler);
        pch_css_set_io_irq(irqnum);
        irq_set_enabled(irqnum, true);
}

void pch_css_configure_io_irq_unused_shared(bool required, uint8_t order_priority) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_io_irq_shared(irqnum, order_priority);
        }
}

void pch_css_configure_io_irq_unused_exclusive(bool required) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_io_irq_exclusive(irqnum);
        }
}

void pch_css_auto_configure_io_irq(bool required) {
        pch_css_configure_io_irq_shared(required,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
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

void pch_css_start(io_callback_t io_callback, uint8_t isc_mask) {
        CSS.isc_enable_mask = isc_mask;

        if (CSS.dmairqix == -1)
                pch_css_auto_configure_dma_irq_index();

        if (CSS.func_irqnum == -1)
                pch_css_auto_configure_func_irq(true);

        if (io_callback) {
                pch_css_set_io_callback(io_callback);
                if (CSS.io_irqnum == -1)
                        pch_css_auto_configure_io_irq(true);
        }
}

bool pch_css_set_trace(bool trace) {
        return pch_trc_set_enable(&CSS.trace_bs, trace);
}

void __time_critical_func(send_tx_packet)(pch_chp_t *chp, proto_packet_t p) {
        DMACHAN_LINK_CMD_COPY(&chp->tx_channel.link, &p);
        chp->tx_active = true;
        dmachan_tx_channel_t *tx = &chp->tx_channel;
        dmachan_link_t *txl = &tx->link;
        dmachan_start_src_cmdbuf(tx);
        if (txl->complete) {
                // packet was sent synchronously via memchan...
                txl->complete = false;
                css_handle_tx_complete(chp);
                // ...but nothing during completion handling should
                // be sending another packet
                assert(!txl->complete);
        }
}
