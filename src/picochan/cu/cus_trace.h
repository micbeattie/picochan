/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CUS_CUS_TRACE_H
#define _PCH_CUS_CUS_TRACE_H

#include "picochan/devib.h"
#include "picochan/cu.h"
#include "picochan/trc_records.h"
#include "trc/trace.h"
#include "proto/packet.h"
#include "txsm/txsm.h"

extern pch_trc_bufferset_t pch_cus_trace_bs;

#define PCH_CUS_TRACE_COND(rt, cond, data) \
        PCH_TRC_WRITE(&pch_cus_trace_bs, (cond), (rt), (data))

#define PCH_CUS_TRACE(rt, data) PCH_CUS_TRACE_COND((rt), true, (data))

static inline void trace_dev(pch_trc_record_type_t rt, pch_devib_t *devib) {
        PCH_CUS_TRACE_COND(rt, cu_or_devib_is_traced(devib),
                ((struct pch_trdata_dev){
                        .cuaddr = pch_dev_get_cuaddr(devib),
                        .ua = pch_dev_get_ua(devib)
                }));
}

static inline void trace_dev_byte(pch_trc_record_type_t rt, pch_devib_t *devib, uint8_t byte) {
        PCH_CUS_TRACE_COND(rt, cu_or_devib_is_traced(devib),
                ((struct pch_trdata_dev_byte){
                        .cuaddr = pch_dev_get_cuaddr(devib),
                        .ua = pch_dev_get_ua(devib),
                        .byte = byte
                }));
}

static inline void trace_dev_packet(pch_trc_record_type_t rt, pch_devib_t *devib, proto_packet_t p) {
        PCH_CUS_TRACE_COND(rt,
                cu_or_devib_is_traced(devib),
                ((struct pch_trdata_word_dev){
                        .word = proto_packet_as_word(p),
                        .cuaddr = pch_dev_get_cuaddr(devib),
                        .ua = pch_dev_get_ua(devib)
                }));
}

static inline void trace_tx_complete(pch_trc_record_type_t rt, pch_cu_t *cu, int16_t uaopt, pch_txsm_state_t txpstate) {
        PCH_CUS_TRACE_COND(rt, cu->traced,
                ((struct pch_trdata_cus_tx_complete){
                        .uaopt = uaopt,
                        .cuaddr = cu->cuaddr,
                        .txpstate = (uint8_t)txpstate
                }));
}

static inline void trace_register_callback(pch_trc_record_type_t rt, pch_cbindex_t n, pch_devib_callback_t cb) {
        PCH_CUS_TRACE(rt,
                ((struct pch_trdata_word_byte){(uint32_t)cb,n}));
}

static inline void trace_call_callback(pch_trc_record_type_t rt, pch_devib_t *devib, pch_cbindex_t cbindex) {
        PCH_CUS_TRACE_COND(rt,
                cu_or_devib_is_traced(devib),
                ((struct pch_trdata_cus_call_callback){
                        .cuaddr = pch_dev_get_cuaddr(devib),
                        .ua = pch_dev_get_ua(devib),
                        .cbindex = (uint8_t)cbindex
                }));
}

static inline void trace_cu_irq(pch_trc_record_type_t rt, pch_cu_t *cu, uint8_t dmairqix, uint8_t tx_irq_state, uint8_t rx_irq_state) {
        PCH_CUS_TRACE_COND(rt,
                cu->traced, ((struct pch_trdata_id_irq){
                        .id = cu->cuaddr,
                        .dmairqix = dmairqix,
                        .tx_state = tx_irq_state << 4
                                | cu->tx_channel.mem_src_state,
                        .rx_state = rx_irq_state << 4
                                | cu->rx_channel.mem_dst_state
                }));
}

#endif
