/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "cu_internal.h"
#include "cus_trace.h"

pch_cu_t *pch_cus[PCH_NUM_CUS];

pch_trc_bufferset_t pch_cus_trace_bs;

static struct async_context_threadsafe_background pch_cus_default_async_context;
async_context_t *pch_cus_async_context;

unsigned char pch_cus_trace_buffer_space[PCH_TRC_NUM_BUFFERS * PCH_TRC_BUFFER_SIZE] __aligned(4);

bool pch_cus_init_done;

void pch_cus_init() {
        assert(!pch_cus_init_done);
        pch_register_devib_callback(PCH_DEVIB_CALLBACK_DEFAULT,
                pch_default_devib_callback, NULL);

        pch_trc_init_bufferset(&pch_cus_trace_bs,
                PCH_CUS_BUFFERSET_MAGIC);
        pch_trc_init_all_buffers(&pch_cus_trace_bs,
                pch_cus_trace_buffer_space);

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_INIT, ((struct {}){}));

        pch_cus_init_done = true;
}

void pch_cu_init(pch_cu_t *cu, uint16_t num_devibs) {
        valid_params_if(PCH_CUS, num_devibs <= PCH_MAX_DEVIBS_PER_CU);

        memset(cu, 0, sizeof(*cu) + num_devibs * sizeof(pch_devib_t));
        pch_devib_list_init(&cu->tx_list);
        pch_devib_list_init(&cu->cb_list);
        cu->rx_active = -1;
        cu->num_devibs = num_devibs;
        cu->irq_index = -1;
}

void pch_cu_register(pch_cu_t *cu, pch_cuaddr_t cua) {
        valid_params_if(PCH_CUS, cua < PCH_NUM_CUS);
        assert(cu->num_devibs > 0);
        assert(!pch_cus[cua]);

        cu->cuaddr = cua;
        pch_cus[cua] = cu;

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_REGISTER,
                ((struct pch_trdata_cu_register){
                        .num_devices = cu->num_devibs,
                        .cuaddr = cua,
                }));
}

static inline void trace_cu_dma(pch_trc_record_type_t rt, pch_cuaddr_t cua, dmachan_link_t *l) {
        PCH_CUS_TRACE(rt, ((struct pch_trdata_dma_init){
                .ctrl = dma_get_ctrl_value(l->dmaid),
                .id = cua,
                .dmaid = l->dmaid,
                .irq_index = l->irq_index,
                .core_num = (uint8_t)get_core_num()
        }));
}

void pch_cus_configure_async_context(async_context_threadsafe_background_config_t *config) {
        async_context_threadsafe_background_config_t default_config = async_context_threadsafe_background_default_config();
        if (!config)
                config = &default_config;

        if (!async_context_threadsafe_background_init(&pch_cus_default_async_context,
                config)) {
                panic("async_context init");
        }

        PCH_CUS_TRACE(PCH_TRC_RT_CUS_INIT_ASYNC_CONTEXT,
                ((struct pch_trdata_id_byte){
                        .id = pch_cus_default_async_context.low_priority_irq_num,
                        .byte = config->low_priority_irq_handler_priority
                }));

        pch_cus_async_context = &pch_cus_default_async_context.core;
}

void pch_cus_configure_async_context_if_unset(void) {
        if (!pch_cus_async_context)
                pch_cus_configure_async_context(NULL);
}

void pch_cu_configure_async_context_if_unset(pch_cu_t *cu) {
        if (cu->async_context)
                return;

        pch_cus_configure_async_context_if_unset();
        cu->async_context = pch_cus_async_context;
}

void pch_cu_configure_irq_index_if_unset(pch_cu_t *cu) {
        if (cu->irq_index == -1) {
                pch_irq_index_t irq_index = pch_cus_find_or_claim_irq_index();
                pch_cu_set_irq_index(cu, irq_index);
        }
}

void pch_cu_configure_dma_irq_if_unset(pch_cu_t *cu) {
        pch_cu_configure_irq_index_if_unset(cu);
        pch_cus_configure_dma_irq_if_unset(cu->irq_index);
}

void pch_cu_configure_pio_irq_if_unset(pch_cu_t *cu, PIO pio) {
        pch_cu_configure_irq_index_if_unset(cu);
        pch_cus_configure_pio_irq_if_unset(pio, cu->irq_index);
}

void pch_cus_uartcu_configure(pch_cuaddr_t cua, uart_inst_t *uart, pch_uartchan_config_t *cfg) {
        pch_cu_t *cu = pch_get_cu(cua);
        assert(!pch_channel_is_started(&cu->channel));
        pch_cu_configure_async_context_if_unset(cu);
        pch_cu_configure_dma_irq_if_unset(cu);

        pch_channel_init_uartchan(&cu->channel, cua, uart, cfg);

        trace_cu_dma(PCH_TRC_RT_CUS_CU_TX_DMA_INIT, cua,
                &cu->channel.tx.link);
        trace_cu_dma(PCH_TRC_RT_CUS_CU_RX_DMA_INIT, cua,
                &cu->channel.rx.link);
}

void pch_cus_piocu_configure(pch_cuaddr_t cua, pch_pio_config_t *cfg, pch_piochan_config_t *pc) {
        pch_cu_t *cu = pch_get_cu(cua);
        assert(!pch_channel_is_started(&cu->channel));
        pch_cu_configure_async_context_if_unset(cu);
        pch_cu_configure_dma_irq_if_unset(cu);
        pch_cu_configure_pio_irq_if_unset(cu, cfg->pio);

        pch_channel_init_piochan(&cu->channel, cua, cfg, pc);

        trace_cu_dma(PCH_TRC_RT_CUS_CU_TX_DMA_INIT, cua,
                &cu->channel.tx.link);
        trace_cu_dma(PCH_TRC_RT_CUS_CU_RX_DMA_INIT, cua,
                &cu->channel.rx.link);
}

void pch_cus_memcu_configure(pch_cuaddr_t cua, pch_channel_t *chpeer) {
        // Check that spin_lock is initialised even when not a Debug
        // release because silently ignoring it produces such
        // nasty-to-troubleshoot race conditions
        dmachan_panic_unless_memchan_initialised();

        pch_cu_t *cu = pch_get_cu(cua);
        assert(!pch_channel_is_started(&cu->channel));

        pch_cu_configure_async_context_if_unset(cu);
        pch_cu_configure_dma_irq_if_unset(cu);

        pch_channel_init_memchan(&cu->channel, cua, cu->irq_index, chpeer);

        trace_cu_dma(PCH_TRC_RT_CUS_CU_TX_DMA_INIT, cua,
                &cu->channel.tx.link);
        trace_cu_dma(PCH_TRC_RT_CUS_CU_RX_DMA_INIT, cua,
                &cu->channel.rx.link);
}

void pch_cu_start(pch_cuaddr_t cua) {
        pch_cu_t *cu = pch_get_cu(cua);
        assert(pch_channel_is_configured(&cu->channel));
        assert(cu->num_devibs > 0);

        if (pch_channel_is_started(&cu->channel))
                return;

        for (int i = 0; i < cu->num_devibs; i++) {
                // point devib at itself to mean "not on any list"
                cu->devibs[i].next = (pch_unit_addr_t)i;
        }

        cu->worker = ((async_when_pending_worker_t){
                .do_work = pch_cus_async_worker_callback,
                .user_data = cu
        });
        async_context_add_when_pending_worker(cu->async_context,
                &cu->worker);

        pch_channel_set_started(&cu->channel, true);
        PCH_CUS_TRACE(PCH_TRC_RT_CUS_CU_STARTED,
                ((struct pch_trdata_id_byte){
                        .id = cua,
                        .byte = 1
                }));

        dmachan_start_dst_reset(&cu->channel.rx);
}

bool pch_cus_set_trace(bool trace) {
        return pch_trc_set_enable(&pch_cus_trace_bs, trace);
}

bool pch_cus_is_traced(void) {
        return pch_cus_trace_bs.enable;
}

uint8_t pch_cu_set_trace_flags(pch_cuaddr_t cua, uint8_t trace_flags) {
        pch_cu_t *cu = pch_get_cu(cua);
        trace_flags &= PCH_CU_TRACED_MASK;
        uint8_t old_trace_flags = pch_cu_trace_flags(cu);
        cu->flags = (cu->flags & ~PCH_CU_TRACED_MASK) | trace_flags;

        if (trace_flags & PCH_CU_TRACED_LINK)
                pch_channel_trace(&cu->channel, &pch_cus_trace_bs);
        else
                pch_channel_trace(&cu->channel, NULL);

        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_CU_TRACED,
                trace_flags != old_trace_flags,
                ((struct pch_trdata_id_byte){
                        .id = cua,
                        .byte = trace_flags
                }));

        return old_trace_flags;
}

bool pch_cus_trace_cu(pch_cuaddr_t cua, bool trace) {
        uint8_t new_trace_flags = trace ? PCH_CU_TRACED_MASK : 0;
        uint8_t old_trace_flags =  pch_cu_set_trace_flags(cua, new_trace_flags);
        return old_trace_flags != new_trace_flags;
}

bool pch_cus_trace_dev(pch_devib_t *devib, bool trace) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        bool old_trace = pch_devib_set_traced(devib, trace);

        PCH_CUS_TRACE_COND(PCH_TRC_RT_CUS_DEV_TRACED,
                pch_cu_is_traced_general(cu) || trace || old_trace,
                ((struct pch_trdata_dev_byte){
                        .cuaddr = cu->cuaddr,
                        .ua = ua,
                        .byte = (uint8_t)trace
                }));

        return old_trace;
}

void pch_cus_trace_write_user(pch_trc_record_type_t rt, void *data, uint8_t data_size) {
        pch_trc_write_raw(&pch_cus_trace_bs, rt, data, data_size);
}

pch_devib_t *__no_inline_not_in_flash_func(pch_cu_pop_devib)(pch_cu_t *cu, pch_devib_list_t *l) {
        uint32_t status = devibs_lock();
        int16_t head = l->head;
        pch_devib_t *devib = NULL;
        if (head != -1) {
                pch_unit_addr_t ua = (pch_unit_addr_t)head;
                devib = pch_get_devib(cu, ua);
                pch_unit_addr_t next = devib->next;

                if (next == ua) {
                        l->head = -1;
                        l->tail = -1;
                } else {
                        l->head = (int16_t)next;
                        devib->next = ua; // remove from list by pointing at self
                }
        }
        devibs_unlock(status);

        return devib;
}

// pch_cu_push_devib pushes devib onto the singly-linked list with
// head and tail l and returns the old tail.
// All manipulation is done under the devibs_lock.
int16_t __no_inline_not_in_flash_func(pch_cu_push_devib)(pch_cu_t *cu, pch_devib_list_t *l, pch_devib_t *devib) {
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        uint32_t status = devibs_lock();
        int16_t tail = l->tail;
        if (tail < 0) {
                l->head = (uint16_t)ua;
                l->tail = (uint16_t)ua;
        } else {
                // There's already a list: add ourselves at the end
                pch_unit_addr_t tail_ua = (pch_unit_addr_t)tail;
                pch_devib_t *tail_devib = pch_get_devib(cu, tail_ua);
                tail_devib->next = ua;
                l->tail = (int16_t)ua;
        }

        devibs_unlock(status);
        return tail;
}
