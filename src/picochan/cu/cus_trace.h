/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CUS_CUS_TRACE_H
#define _PCH_CUS_CUS_TRACE_H

#include "picochan/devib.h"
#include "picochan/cu.h"
#include "trc/trace.h"
#include "proto/packet.h"
#include "txsm/txsm.h"

extern pch_trc_bufferset_t pch_cus_trace_bs;

struct pch_cus_trdata_init_mem_channel {
        pch_cunum_t     cunum;
        pch_dmaid_t     txdmaid;
        pch_dmaid_t     rxdmaid;
};

struct pch_cus_trdata_tx_complete {
        int16_t         uaopt;
        pch_cunum_t     cunum;
        uint8_t         txpstate;
};

struct pch_cus_trdata_call_callback {
        pch_cunum_t     cunum;
        pch_unit_addr_t ua;
        uint8_t         cbindex;
};

#define PCH_CUS_TRACE_COND(rt, cond, data) \
        PCH_TRC_WRITE(&pch_cus_trace_bs, (cond), (rt), (data))

#define PCH_CUS_TRACE(rt, data) PCH_CUS_TRACE_COND((rt), true, (data))

static inline void trace_dev(pch_trc_record_type_t rt, pch_cu_t *cu, pch_devib_t *devib) {
        PCH_CUS_TRACE_COND(rt, cu_or_devib_is_traced(cu, devib),
                ((struct pch_trc_trdata_dev){
                        .cunum = cu->cunum,
                        .ua = pch_get_ua(cu, devib),
                }));
}

static inline void trace_dev_byte(pch_trc_record_type_t rt, pch_cu_t *cu, pch_devib_t *devib, uint8_t byte) {
        PCH_CUS_TRACE_COND(rt, cu_or_devib_is_traced(cu, devib),
                ((struct pch_trc_trdata_dev_byte){
                        .cunum = cu->cunum,
                        .ua = pch_get_ua(cu, devib),
                        .byte = byte
                }));
}

static inline void trace_dev_packet(pch_trc_record_type_t rt, pch_cu_t *cu, pch_devib_t *devib, proto_packet_t p) {
        PCH_CUS_TRACE_COND(rt,
                cu_or_devib_is_traced(cu, devib),
                ((struct pch_trc_trdata_word_dev){
                        .word = proto_packet_as_word(p),
                        .cunum = cu->cunum,
                        .ua = pch_get_ua(cu, devib)
                }));
}

static inline void trace_tx_complete(pch_trc_record_type_t rt, pch_cu_t *cu, int16_t uaopt, pch_txsm_state_t txpstate) {
        PCH_CUS_TRACE_COND(rt, cu->traced,
                ((struct pch_cus_trdata_tx_complete){
                        .uaopt = uaopt,
                        .cunum = cu->cunum,
                        .txpstate = (uint8_t)txpstate
                }));
}

static inline void trace_register_callback(pch_trc_record_type_t rt, pch_cbindex_t n, pch_devib_callback_t cb) {
        PCH_CUS_TRACE(rt,
                ((struct pch_trc_trdata_word_byte){(uint32_t)cb,n}));
}

static inline void trace_call_callback(pch_trc_record_type_t rt, pch_cu_t *cu, pch_devib_t *devib, pch_cbindex_t cbindex) {
        PCH_CUS_TRACE_COND(rt,
                cu_or_devib_is_traced(cu, devib),
                ((struct pch_cus_trdata_call_callback){
                        .cunum = cu->cunum,
                        .ua = pch_get_ua(cu, devib),
                        .cbindex = (uint8_t)cbindex
                }));
}

static inline void trace_cus_cu_irq(pch_trc_record_type_t rt, pch_cu_t *cu, uint8_t dmairqix, bool tx_irq_raised, bool rx_irq_raised) {
        PCH_CUS_TRACE_COND(rt,
                cu->traced, ((struct pch_trc_trdata_cu_irq){
                        .cunum = cu->cunum,
                        .dmairqix = dmairqix,
                        .tx_state = ((uint8_t)tx_irq_raised << 4)
                                | cu->tx_channel.mem_src_state,
                        .rx_state = ((uint8_t)rx_irq_raised << 4)
                                | cu->rx_channel.mem_dst_state
                }));
}

#endif
