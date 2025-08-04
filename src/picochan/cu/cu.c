/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include <string.h>
#include "picochan/cu.h"
#include "cus_trace.h"

pch_cu_t *pch_cus[PCH_NUM_CUS];

pch_trc_bufferset_t pch_cus_trace_bs;

unsigned char pch_cus_trace_buffer_space[PCH_TRC_NUM_BUFFERS * PCH_TRC_BUFFER_SIZE] __aligned(4);

bool pch_cus_init_done;

void pch_cus_init() {
        assert(!pch_cus_init_done);
        pch_register_devib_callback(PCH_DEVIB_CALLBACK_DEFAULT,
                pch_default_devib_callback);

        pch_trc_init_bufferset(&pch_cus_trace_bs,
                PCH_CUS_BUFFERSET_MAGIC);
        pch_trc_init_all_buffers(&pch_cus_trace_bs,
                pch_cus_trace_buffer_space);
        pch_cus_init_done = true;
}

// CU interrupts and callbacks will be handled on the core that calls
// this function
void pch_cus_init_dma_irq_handler(pch_dma_irq_index_t dmairqix) {
        assert(dmairqix >= 0 && dmairqix < NUM_DMA_IRQS);
        irq_num_t irqnum = dma_get_irq_num((uint)dmairqix);
        irq_add_shared_handler(irqnum, pch_cus_handle_dma_irq,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(irqnum, true);
        
        PCH_CUS_TRACE(PCH_TRC_RT_CUS_INIT_DMA_IRQ_HANDLER,
                ((struct pch_trdata_word_byte){
                        (uint32_t)pch_cus_handle_dma_irq, irqnum}));
}

void pch_cu_init(pch_cu_t *cu, pch_cuaddr_t cua, pch_dma_irq_index_t dmairqix, uint16_t num_devibs) {
        valid_params_if(PCH_CUS, cua < PCH_NUM_CUS);
        valid_params_if(PCH_CUS,
                dmairqix >= 0 && dmairqix < NUM_DMA_IRQS);
        valid_params_if(PCH_CUS, num_devibs <= PCH_MAX_DEVIBS_PER_CU);

        memset(cu, 0, sizeof(*cu) + num_devibs * sizeof(pch_devib_t));
        cu->cuaddr = cua;
        cu->rx_active = -1;
        cu->tx_head = -1;
        cu->tx_tail = -1;
        cu->dmairqix = dmairqix;
        cu->num_devibs = num_devibs;

        for (int i = 0; i < num_devibs; i++) {
                // point devib at itself to mean "not on rx list"
                cu->devibs[i].next = (pch_unit_addr_t)i;
        }

        pch_cus[cua] = cu;

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_INIT,
                ((struct pch_trdata_cu_init){
                        .num_devices = num_devibs,
                        .cuaddr = cua,
                        .dmairqix = dmairqix
                }));
}

dmachan_tx_channel_t *pch_cu_get_tx_channel(pch_cuaddr_t cua) {
        pch_cu_t *cu = pch_get_cu(cua);
        return &cu->tx_channel;
}

dmachan_rx_channel_t *pch_cu_get_rx_channel(pch_cuaddr_t cua) {
        pch_cu_t *cu = pch_get_cu(cua);
        return &cu->rx_channel;
}

static inline void trace_cu_dma(pch_trc_record_type_t rt, pch_cuaddr_t cua, dmachan_1way_config_t *d1c) {
        PCH_CUS_TRACE(rt, ((struct pch_trdata_dma_init){
                .addr = d1c->addr,
                .ctrl = channel_config_get_ctrl_value(&d1c->ctrl),
                .id = cua,
                .dmaid = d1c->dmaid,
                .dmairqix = d1c->dmairqix
        }));
}

static void cu_dma_tx_init(pch_cuaddr_t cua, dmachan_1way_config_t *d1c) {
        pch_cu_t *cu = pch_get_cu(cua);
        dmachan_init_tx_channel(&cu->tx_channel, d1c);
        trace_cu_dma(PCH_TRC_RT_CUS_CU_TX_DMA_INIT, cua, d1c);
}

static void cu_dma_rx_init(pch_cuaddr_t cua, dmachan_1way_config_t *d1c) {
        pch_cu_t *cu = pch_get_cu(cua);
        dmachan_init_rx_channel(&cu->rx_channel, d1c);
        trace_cu_dma(PCH_TRC_RT_CUS_CU_RX_DMA_INIT, cua, d1c);
}

void pch_cu_dma_configure(pch_cuaddr_t cua, dmachan_config_t *dc) {
        pch_cu_t *cu = pch_get_cu(cua);
        assert(!cu->started);
        (void)cu;

        cu_dma_tx_init(cua, &dc->tx);
        cu_dma_rx_init(cua, &dc->rx);
}

void pch_cu_set_configured(pch_cuaddr_t cua, bool configured) {
        pch_cu_t *cu = pch_get_cu(cua);

        cu->configured = true;

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_CONFIGURED,
                ((struct pch_trdata_id_byte){
                        .id = cua,
                        .byte = (uint8_t)configured
                }));
}

void pch_cus_uartcu_configure(pch_cuaddr_t cua, uart_inst_t *uart, dma_channel_config ctrl) {
        dma_channel_config txctrl = dmachan_uart_make_txctrl(uart, ctrl);
        dma_channel_config rxctrl = dmachan_uart_make_rxctrl(uart, ctrl);
        uint32_t hwaddr = (uint32_t)&uart_get_hw(uart)->dr; // read/write fifo
        pch_cu_t *cu = pch_get_cu(cua);
        dmachan_config_t dc = dmachan_config_claim(hwaddr, txctrl,
                hwaddr, rxctrl, cu->dmairqix);

        pch_cu_dma_configure(cua, &dc);
        dmachan_set_link_irq_enabled(&cu->tx_channel.link, true);
        dmachan_set_link_irq_enabled(&cu->rx_channel.link, true);
        pch_cu_set_configured(cua, true);
}

void pch_cus_auto_configure_uartcu(pch_cuaddr_t cua, uart_inst_t *uart, uint baudrate) {
        pch_uart_init(uart, baudrate);

        // Argument 0 is ok here (as would be any DMA id) because it
        // only affects the "chain-to" value and that is overridden in
        // pch_css_uartcu_configure anyway.
        dma_channel_config ctrl = dma_channel_get_default_config(0);
        pch_cus_uartcu_configure(cua, uart, ctrl);
}

void pch_cus_memcu_configure(pch_cuaddr_t cua, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer) {
        // Check that spin_lock is initialised even when not a Debug
        // release because silently ignoring it produces such
        // nasty-to-troubleshoot race conditions
        dmachan_panic_unless_memchan_initialised();

        pch_cu_t *cu = pch_get_cu(cua);
        assert(!cu->started);

        dmachan_config_t dc = dmachan_config_memchan_make(txdmaid,
                rxdmaid, cu->dmairqix);
        pch_cu_dma_configure(cua, &dc);
        // Do not enable irq for tx channel link because Pico DMA
        // does not treat the INTSn bits separately. We enable only
        // the rx side for irqs and the rx irq handler propagates
        // notifications to the tx side via the INTFn "forced irq"
        // register which overrides the INTEn enabled bits.
        dmachan_rx_channel_t *rx = &cu->rx_channel;
        dmachan_set_link_irq_enabled(&rx->link, true);
        txpeer->mem_rx_peer = rx;
        rx->mem_tx_peer = txpeer;
        pch_cu_set_configured(cua, true);
}

void pch_cu_start(pch_cuaddr_t cua) {
        pch_cu_t *cu = pch_get_cu(cua);
        assert(cu->configured);

        if (cu->started)
                return;

        cu->started = true;
        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_STARTED,
                ((struct pch_trdata_id_byte){
                        .id = cua,
                        .byte = 1
                }));

        dmachan_start_dst_reset(&cu->rx_channel);
}

bool pch_cus_set_trace(bool trace) {
        return pch_trc_set_enable(&pch_cus_trace_bs, trace);
}

bool pch_cus_trace_cu(pch_cuaddr_t cua, bool trace) {
        pch_cu_t *cu = pch_get_cu(cua);
        bool old_trace = cu->traced;
        cu->traced = trace;
        pch_trc_bufferset_t *bs = trace ? &pch_cus_trace_bs : 0;

        dmachan_set_link_bs(&cu->tx_channel.link, bs);
        dmachan_set_link_bs(&cu->rx_channel.link, bs);

        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_CU_TRACED,
                trace || old_trace,
                ((struct pch_trdata_id_byte){
                        .id = cua,
                        .byte = (uint8_t)trace
                }));

        return old_trace;
}

bool pch_cus_trace_dev(pch_devib_t *devib, bool trace) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        bool old_trace = pch_devib_set_traced(devib, trace);

        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_DEV_TRACED,
                cu->traced || trace || old_trace,
                ((struct pch_trdata_dev_byte){
                        .cuaddr = cu->cuaddr,
                        .ua = ua,
                        .byte = (uint8_t)trace
                }));

        return old_trace;
}
