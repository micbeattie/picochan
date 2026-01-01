/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
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
        CSS.irq_index = -1; // CSS not yet started
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

	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_CORE_NUM,
                ((struct pch_trdata_byte){(uint8_t)core_num}));
}

static void trace_set_irq_handler(pch_trc_record_type_t rt, irq_num_t irqnum, irq_handler_t handler, int16_t order_priority_opt) {
        PCH_CSS_TRACE(rt, ((struct pch_trdata_irq_handler){
                        .handler = (uint32_t)handler,
                        .order_priority = order_priority_opt,
                        .irqnum = (uint8_t)irqnum
                }));
}

static void configure_irq_handler(uint irqnum, irq_handler_t handler, int order_priority) {
        css_try_set_core_num();
        if (order_priority == -1)
                irq_set_exclusive_handler(irqnum, handler);
        else
                irq_add_shared_handler(irqnum, handler, order_priority);

        irq_set_enabled(irqnum, true);
        trace_set_irq_handler(PCH_TRC_RT_CSS_INIT_IRQ_HANDLER,
                irqnum, handler, order_priority);
}

int8_t pch_css_get_core_num(void) {
        return CSS.core_num;
}

// Configuring CSS IRQ index

pch_irq_index_t pch_css_get_irq_index(void) {
        return CSS.irq_index;
}

void pch_css_set_irq_index(pch_irq_index_t irq_index) {
        if (irq_index < 0 || irq_index >= NUM_IRQ_INDEXES)
                panic("invalid IRQ index");

	PCH_CSS_TRACE(PCH_TRC_RT_CSS_SET_IRQ_INDEX,
                ((struct pch_trdata_byte){irq_index}));
        assert(CSS.irq_index == -1 || CSS.irq_index == irq_index);
        CSS.irq_index = irq_index;
}

void pch_css_set_irq_index_if_needed(void) {
        if (CSS.irq_index == -1)
                pch_css_set_irq_index(get_core_num());
}

// DMA interrupt

static void configure_dma_irq(int order_priority) {
        assert(!CSS.dma_irq_configured);
        pch_css_set_irq_index_if_needed();
        irq_num_t irqnum = dma_get_irq_num(CSS.irq_index);
        configure_irq_handler(irqnum, pch_css_dma_irq_handler,
                order_priority);
        CSS.dma_irq_configured = true;
}

void pch_css_configure_dma_irq_shared(uint8_t order_priority) {
        configure_dma_irq(order_priority);
}

void pch_css_configure_dma_irq_exclusive(void) {
        configure_dma_irq(-1);
}

void pch_css_configure_dma_irq_shared_default(void) {
        configure_dma_irq(PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
}

void pch_css_configure_dma_irq_if_needed(void) {
        if (!CSS.dma_irq_configured)
                pch_css_configure_dma_irq_shared_default();
}

// PIO interrupts

static void configure_pio_irq(PIO pio, int order_priority) {
        uint pio_num = PIO_NUM(pio);
        assert(!CSS.pio_irq_configured[pio_num]);
        pch_css_set_irq_index_if_needed();
        irq_num_t irqnum = pio_get_irq_num(pio, CSS.irq_index);
        configure_irq_handler(irqnum, pch_css_pio_irq_handler,
                order_priority);
        CSS.pio_irq_configured[pio_num] = true;
}

void pch_css_configure_pio_irq_shared(PIO pio, uint8_t order_priority)
 {
        configure_pio_irq(pio, order_priority);
}

void pch_css_configure_pio_irq_exclusive(PIO pio) {
        configure_pio_irq(pio, -1);
}

void pch_css_configure_pio_irq_shared_default(PIO pio) {
        configure_pio_irq(pio, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
}

void pch_css_configure_pio_irq_if_needed(PIO pio) {
        if (!CSS.pio_irq_configured[PIO_NUM(pio)])
                pch_css_configure_pio_irq_shared_default(pio);
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

void pch_css_configure_func_irq_exclusive(irq_num_t irqnum) {
        pch_css_set_func_irq(irqnum);
        configure_irq_handler(irqnum, pch_css_func_irq_handler, -1);
}

void pch_css_configure_func_irq_shared(irq_num_t irqnum, uint8_t order_priority) {
        pch_css_set_func_irq(irqnum);
        configure_irq_handler(irqnum, pch_css_func_irq_handler,
                order_priority);
        irq_set_enabled(irqnum, true);
}

void pch_css_configure_func_irq_shared_default(irq_num_t irqnum) {
        pch_css_configure_func_irq_shared(irqnum,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
}

bool pch_css_configure_func_irq_unused_exclusive(bool required) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_func_irq_exclusive(irqnum);
                return true;
        }

        return false;
}

bool pch_css_configure_func_irq_unused_shared(bool required, uint8_t order_priority) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_func_irq_shared(irqnum, order_priority);
                return true;
        }

        return false;
}

bool pch_css_configure_func_irq_unused_shared_default(bool required) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_func_irq_shared_default(irqnum);
                return true;
        }

        return false;
}

void pch_css_auto_configure_func_irq(void) {
        pch_css_configure_func_irq_unused_shared_default(true);
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

void pch_css_configure_io_irq_exclusive(irq_num_t irqnum) {
        pch_css_set_io_irq(irqnum);
        configure_irq_handler(irqnum, pch_css_func_irq_handler, -1);
}

void pch_css_configure_io_irq_shared(irq_num_t irqnum, uint8_t order_priority) {
        pch_css_set_io_irq(irqnum);
        configure_irq_handler(irqnum, pch_css_func_irq_handler,
                order_priority);
}

void pch_css_configure_io_irq_shared_default(irq_num_t irqnum) {
        pch_css_configure_io_irq_shared(irqnum,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
}

bool pch_css_configure_io_irq_unused_exclusive(bool required) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_io_irq_exclusive(irqnum);
                return true;
        }

        return false;
}

bool pch_css_configure_io_irq_unused_shared(bool required, uint8_t order_priority) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_io_irq_shared(irqnum, order_priority);
                return true;
        }

        return false;
}

bool pch_css_configure_io_irq_unused_shared_default(bool required) {
        int irqnum_opt = user_irq_claim_unused(required);
        if (irqnum_opt != -1) {
                irq_num_t irqnum = (irq_num_t)irqnum_opt;
                pch_css_configure_io_irq_shared_default(irqnum);
                return true;
        }

        return false;
}

void pch_css_auto_configure_io_irq(void) {
        pch_css_configure_io_irq_unused_shared_default(true);
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

        pch_css_set_irq_index_if_needed();

        if (CSS.func_irqnum == -1)
                pch_css_auto_configure_func_irq();

        if (io_callback) {
                pch_css_set_io_callback(io_callback);
                if (CSS.io_irqnum == -1)
                        pch_css_auto_configure_io_irq();
        }
}

bool pch_css_set_trace(bool trace) {
        return pch_trc_set_enable(&CSS.trace_bs, trace);
}

void __time_critical_func(send_tx_packet)(pch_chp_t *chp, pch_schib_t *schib, proto_packet_t p) {
        dmachan_tx_channel_t *tx = &chp->channel.tx;
        dmachan_link_t *txl = &tx->link;
        uint32_t cmd = proto_packet_as_word(p);
        dmachan_link_cmd_set(txl, dmachan_make_cmd_from_word(cmd));
        trace_schib_packet(PCH_TRC_RT_CSS_SEND_TX_PACKET, schib, p,
                dmachan_link_seqnum(txl));
        pch_chp_set_tx_active(chp, true);
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

void __time_critical_func(pch_css_trace_write_user)(pch_trc_record_type_t rt, void *data, uint8_t data_size) {
        assert(rt >= PCH_TRC_RT_USER_FIRST);
        pch_trc_write_raw(&CSS.trace_bs, rt, data, data_size);
}
