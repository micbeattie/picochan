/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CSS_CSS_TRACE_H
#define _PCH_CSS_CSS_TRACE_H

#include "css_internal.h"

#include "trc/trace.h"

struct trdata_css_cu_claim {
        pch_sid_t       first_sid;
        uint16_t        num_devices;
        pch_cunum_t     cunum;
};

struct trdata_irqnum_opt {
        int16_t         irqnum_opt;
};

struct trdata_address_change {
        uint32_t        old_addr;
        uint32_t        new_addr;
};

struct trdata_func_irq {
        int16_t         ua_opt;
        pch_cunum_t     cunum;
        uint8_t         tx_active;
};

#define PCH_CSS_TRACE_COND(rt, cond, data) \
        PCH_TRC_WRITE(&CSS.trace_bs, (cond), (rt), (data))

#define PCH_CSS_TRACE(rt, data) PCH_CSS_TRACE_COND((rt), true, (data))

static inline void trace_schib_byte(pch_trc_record_type_t rt, pch_schib_t *schib, uint8_t byte) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trc_trdata_sid_byte){get_sid(schib), byte}));
}

static inline void trace_schib_word_byte(pch_trc_record_type_t rt, pch_schib_t *schib, uint32_t word, uint8_t byte) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trc_trdata_word_sid_byte){word, get_sid(schib), byte}));
}

static inline void trace_schib_packet(pch_trc_record_type_t rt, pch_schib_t *schib, proto_packet_t p) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trc_trdata_word_sid){proto_packet_as_word(p), get_sid(schib)}));
}

static inline void trace_schib_ccw(pch_trc_record_type_t rt, pch_schib_t *schib, pch_ccw_t *ccw_addr, pch_ccw_t ccw) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trc_trdata_ccw_addr_sid){
                        .ccw = ccw,
                        .addr = (uint32_t)ccw_addr,
                        .sid = get_sid(schib)
                }));
}

static inline void trace_schib_callback(pch_trc_record_type_t rt, pch_schib_t *schib, pch_intcode_t *ic) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trc_trdata_intcode_scsw){
                        .intcode = *ic,
                        .scsw = schib->scsw,
                }));
}

static inline void trace_schib_scsw_cc(pch_trc_record_type_t rt, pch_schib_t *schib, pch_scsw_t *scsw, uint8_t cc) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trc_trdata_scsw_sid_cc){
                        .scsw = *scsw,
                        .sid = get_sid(schib),
                        .cc = cc
                }));
}

static inline void trace_css_cu_irq(pch_trc_record_type_t rt, css_cu_t *cu, uint8_t dmairqix, uint8_t tx_irq_reason, uint8_t rx_irq_reason) {
        PCH_CSS_TRACE_COND(rt,
                cu->traced, ((struct pch_trc_trdata_cu_irq){
                        .cunum = cu->cunum,
                        .dmairqix = dmairqix,
                        .tx_state = tx_irq_reason << 4
                                | cu->tx_channel.mem_src_state,
                        .rx_state = rx_irq_reason << 4
                                | cu->rx_channel.mem_dst_state
                }));
}

#endif
