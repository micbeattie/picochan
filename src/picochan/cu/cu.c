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

void pch_cus_init_dma_irq_handler(uint8_t dmairqix) {
        irq_num_t irqnum = dma_get_irq_num(dmairqix);
        irq_add_shared_handler(irqnum, pch_cus_handle_dma_irq,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(irqnum, true);
        
        PCH_CUS_TRACE(PCH_TRC_RT_CUS_INIT_DMA_IRQ_HANDLER,
                ((struct pch_trc_trdata_word_byte){
                        (uint32_t)pch_cus_handle_dma_irq, irqnum}));
}

void pch_cus_register_cu(pch_cu_t *cu, pch_cunum_t cunum, uint8_t dmairqix, uint16_t num_devibs) {
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

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_REGISTER_CU,
                ((struct pch_cus_trdata_register_cu){
                        .num_devices = num_devibs,
                        .cunum = cunum,
                        .dmairqix = dmairqix
                }));
}

void pch_cus_init_channel(pch_cunum_t cunum, uint32_t txhwaddr, dma_channel_config txctrl, uint32_t rxhwaddr, dma_channel_config rxctrl) {
        pch_cu_t *cu = pch_get_cu(cunum);

        pch_dmaid_t txdmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        dmachan_init_tx_channel(&cu->tx_channel, txdmaid, txhwaddr, txctrl);
        dma_irqn_set_channel_enabled(cu->dmairqix, txdmaid, true);
        trace_init_channel(PCH_TRC_RT_CUS_INIT_TX_CHANNEL, cunum,
                0, txctrl, txdmaid);

        pch_dmaid_t rxdmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        dmachan_init_rx_channel(&cu->rx_channel, rxdmaid, rxhwaddr, rxctrl);
        dma_irqn_set_channel_enabled(cu->dmairqix, rxdmaid, true);
        trace_init_channel(PCH_TRC_RT_CUS_INIT_RX_CHANNEL,
                cunum, rxhwaddr, rxctrl, rxdmaid);
}

void pch_cus_init_mem_channel(pch_cunum_t cunum, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid) {
        pch_cu_t *cu = pch_get_cu(cunum);

        dma_channel_config czero = {0}; // zero, *not* default config

        dmachan_init_tx_channel(&cu->tx_channel, txdmaid, 0, czero);
        dma_irqn_set_channel_enabled(cu->dmairqix, txdmaid, true);

        dmachan_init_rx_channel(&cu->rx_channel, rxdmaid, 0, czero);
        dma_irqn_set_channel_enabled(cu->dmairqix, rxdmaid, true);

        trace_init_mem_channel(PCH_TRC_RT_CUS_INIT_MEM_CHANNEL,
                cunum, txdmaid, rxdmaid);
}

void pch_cus_enable_cu(pch_cunum_t cunum) {
        pch_cu_t *cu = pch_get_cu(cunum);
        if (cu->enabled)
                return;

        cu->enabled = true;
        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_ENABLE_CU,
                cu->traced, ((struct{}){}));

        dmachan_start_dst_cmdbuf(&cu->rx_channel);
}

bool pch_cus_set_trace(bool trace) {
        return pch_trc_set_enable(&pch_cus_trace_bs, trace);
}

bool pch_cus_trace_cu(pch_cunum_t cunum, bool trace) {
        pch_cu_t *cu = pch_get_cu(cunum);
        bool old_trace = cu->traced;
        cu->traced = trace;
        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_SET_TRACE_CU,
                trace || old_trace,
                ((struct pch_cus_trdata_cunum_traceold_tracenew){
                        .cunum = cunum,
                        .old_trace = old_trace,
                        .new_trace = trace
                }));

        return old_trace;
}

bool pch_cus_trace_dev(pch_cunum_t cunum, pch_unit_addr_t ua, bool trace) {
        pch_cu_t *cu = pch_get_cu(cunum);
        pch_devib_t *devib = pch_get_devib(cu, ua);
        bool old_trace = pch_devib_set_traced(devib, trace);

        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_SET_TRACE_CU,
                cu->traced || trace || old_trace,
                ((struct pch_cus_trdata_dev_traceold_tracenew){
                        .cunum = cunum,
                        .ua = ua,
                        .old_trace = old_trace,
                        .new_trace = trace
                }));

        return old_trace;
}
