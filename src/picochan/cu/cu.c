/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include <string.h>
#include "picochan/cu.h"
#include "cus_trace.h"

pch_cu_t *pch_cus[NUM_CUS];

pch_trc_bufferset_t pch_cus_trace_bs;

unsigned char pch_cus_trace_buffer_space[PCH_TRC_NUM_BUFFERS * PCH_TRC_BUFFER_SIZE] __aligned(4);

void pch_cus_init() {
        pch_register_devib_callback(PCH_DEVIB_CALLBACK_DEFAULT,
                pch_default_devib_callback);

        pch_trc_init_bufferset(&pch_cus_trace_bs,
                        PCH_CUS_BUFFERSET_MAGIC);
        pch_trc_init_all_buffers(&pch_cus_trace_bs,
                pch_cus_trace_buffer_space);
}

// CU interrupts and callbacks will be handled on the core that calls
// this function
void pch_cus_init_dma_irq_handler(uint8_t dmairqix) {
        irq_num_t irqnum = dma_get_irq_num(dmairqix);
        irq_add_shared_handler(irqnum, pch_cus_handle_dma_irq,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(irqnum, true);
        
        PCH_CUS_TRACE(PCH_TRC_RT_CUS_INIT_DMA_IRQ_HANDLER,
                ((struct pch_trc_trdata_word_byte){
                        (uint32_t)pch_cus_handle_dma_irq, irqnum}));
}

void pch_cus_cu_init(pch_cu_t *cu, pch_cunum_t cunum, uint8_t dmairqix, uint16_t num_devibs) {
        valid_params_if(PCH_CUS, cunum < NUM_CUS);
        valid_params_if(PCH_CUS, dmairqix < NUM_DMA_IRQS);
        valid_params_if(PCH_CUS, num_devibs < NUM_DEVIBS);

        memset(cu, 0, sizeof(*cu) + num_devibs * sizeof(pch_devib_t));
        cu->cunum = cunum;
        cu->rx_active = -1;
        cu->tx_head = -1;
        cu->tx_tail = -1;
        cu->dmairqix = dmairqix;
        cu->num_devibs = num_devibs;

        for (int i = 0; i < num_devibs; i++) {
                // point devib at itself to mean "not on rx list"
                cu->devibs[i].next = (pch_unit_addr_t)i;
        }

        pch_cus[cunum] = cu;

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_INIT,
                ((struct pch_trc_trdata_cu_init){
                        .num_devices = num_devibs,
                        .cunum = cunum,
                        .dmairqix = dmairqix
                }));
}

dmachan_tx_channel_t *pch_cus_cu_get_tx_channel(pch_cunum_t cunum) {
        pch_cu_t *cu = pch_get_cu(cunum);
        return &cu->tx_channel;
}

dmachan_rx_channel_t *pch_cus_cu_get_rx_channel(pch_cunum_t cunum) {
        pch_cu_t *cu = pch_get_cu(cunum);
        return &cu->rx_channel;
}

static inline void trace_cu_dma(pch_trc_record_type_t rt, pch_cunum_t cunum, dmachan_1way_config_t *d1c) {
        PCH_CUS_TRACE(rt, ((struct pch_trc_trdata_cu_dma){
                .addr = d1c->addr,
                .ctrl = channel_config_get_ctrl_value(&d1c->ctrl),
                .cunum = cunum,
                .dmaid = d1c->dmaid,
                .dmairqix = d1c->dmairqix
        }));
}

static void cu_dma_tx_init(pch_cunum_t cunum, dmachan_1way_config_t *d1c) {
        pch_cu_t *cu = pch_get_cu(cunum);
        dmachan_init_tx_channel(&cu->tx_channel, d1c);
        trace_cu_dma(PCH_TRC_RT_CUS_CU_TX_DMA_INIT, cunum, d1c);
}

static void cu_dma_rx_init(pch_cunum_t cunum, dmachan_1way_config_t *d1c) {
        pch_cu_t *cu = pch_get_cu(cunum);
        dmachan_init_rx_channel(&cu->rx_channel, d1c);
        trace_cu_dma(PCH_TRC_RT_CUS_CU_RX_DMA_INIT, cunum, d1c);
}

void pch_cus_cu_dma_configure(pch_cunum_t cunum, dmachan_config_t *dc) {
        pch_cu_t *cu = pch_get_cu(cunum);
        assert(!cu->started);

        cu_dma_tx_init(cunum, &dc->tx);
        cu_dma_rx_init(cunum, &dc->rx);
}

void pch_cus_cu_set_configured(pch_cunum_t cunum, bool configured) {
        pch_cu_t *cu = pch_get_cu(cunum);

        cu->configured = true;

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_CONFIGURED,
                ((struct pch_trc_trdata_cu_byte){
                        .cunum = cunum,
                        .byte = (uint8_t)configured
                }));
}

void pch_cus_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl) {
        pch_init_uart(uart);

        dma_channel_config txctrl = dmachan_uartcu_make_txctrl(uart, ctrl);
        dma_channel_config rxctrl = dmachan_uartcu_make_rxctrl(uart, ctrl);
        uint32_t hwaddr = (uint32_t)&uart_get_hw(uart)->dr; // read/write fifo
        pch_cu_t *cu = pch_get_cu(cunum);
        dmachan_config_t dc = dmachan_config_claim(hwaddr, txctrl,
                hwaddr, rxctrl, cu->dmairqix);

        pch_cus_cu_dma_configure(cunum, &dc);
        dmachan_set_tx_channel_irq_enabled(&cu->tx_channel, true);
        dmachan_set_rx_channel_irq_enabled(&cu->rx_channel, true);
        pch_cus_cu_set_configured(cunum, true);
}

void pch_cus_memcu_configure(pch_cunum_t cunum, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer) {
        // Check that spin_lock is initialised even when not a Debug
        // release because silently ignoring it produces such
        // nasty-to-troubleshoot race conditions
        dmachan_panic_unless_memchan_initialised();

        pch_cu_t *cu = pch_get_cu(cunum);
        assert(!cu->started);

        dmachan_config_t dc = dmachan_config_memchan_make(txdmaid,
                rxdmaid, cu->dmairqix);
        pch_cus_cu_dma_configure(cunum, &dc);

        dmachan_set_tx_channel_irq_enabled(&cu->tx_channel, true);
        dmachan_rx_channel_t *rx = &cu->rx_channel;
        dmachan_set_rx_channel_irq_enabled(rx, true);
        txpeer->mem_rx_peer = rx;
        rx->mem_tx_peer = txpeer;
        pch_cus_cu_set_configured(cunum, true);
}

void pch_cus_cu_start(pch_cunum_t cunum) {
        pch_cu_t *cu = pch_get_cu(cunum);
        assert(cu->configured);

        if (cu->started)
                return;

        cu->started = true;
        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_STARTED,
                ((struct pch_trc_trdata_cu_byte){
                        .cunum = cunum,
                        .byte = 1
                }));

        dmachan_start_dst_cmdbuf(&cu->rx_channel);
}

bool pch_cus_set_trace(bool trace) {
        return pch_trc_set_enable(&pch_cus_trace_bs, trace);
}

bool pch_cus_trace_cu(pch_cunum_t cunum, bool trace) {
        pch_cu_t *cu = pch_get_cu(cunum);
        bool old_trace = cu->traced;
        cu->traced = trace;
        if (trace) {
                cu->tx_channel.bs = &pch_cus_trace_bs;
                cu->rx_channel.bs = &pch_cus_trace_bs;
        } else {
                cu->tx_channel.bs = 0;
                cu->rx_channel.bs = 0;
        }

        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_CU_TRACED,
                trace || old_trace,
                ((struct pch_trc_trdata_cu_byte){
                        .cunum = cunum,
                        .byte = (uint8_t)trace
                }));

        return old_trace;
}

bool pch_cus_trace_dev(pch_cunum_t cunum, pch_unit_addr_t ua, bool trace) {
        pch_cu_t *cu = pch_get_cu(cunum);
        pch_devib_t *devib = pch_get_devib(cu, ua);
        bool old_trace = pch_devib_set_traced(devib, trace);

        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_DEV_TRACED,
                cu->traced || trace || old_trace,
                ((struct pch_trc_trdata_dev_byte){
                        .cunum = cunum,
                        .ua = ua,
                        .byte = (uint8_t)trace
                }));

        return old_trace;
}
