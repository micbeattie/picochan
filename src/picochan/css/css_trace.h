/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CSS_CSS_TRACE_H
#define _PCH_CSS_CSS_TRACE_H

#include "css_internal.h"

#include "picochan/trc_records.h"
#include "trc/trace.h"

#define PCH_CSS_TRACE_COND(rt, cond, data) \
        PCH_TRC_WRITE(&CSS.trace_bs, (cond), (rt), (data))

#define PCH_CSS_TRACE(rt, data) PCH_CSS_TRACE_COND((rt), true, (data))

static inline void trace_schib_byte(pch_trc_record_type_t rt, pch_schib_t *schib, uint8_t byte) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trdata_sid_byte){get_sid(schib), byte}));
}

static inline void trace_schib_word_byte(pch_trc_record_type_t rt, pch_schib_t *schib, uint32_t word, uint8_t byte) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trdata_word_sid_byte){word, get_sid(schib), byte}));
}

static inline void trace_schib_packet(pch_trc_record_type_t rt, pch_schib_t *schib, proto_packet_t p, uint16_t seqnum) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trdata_packet_sid){
                        .packet = proto_packet_as_word(p),
                        .sid = get_sid(schib),
                        .seqnum = seqnum
                }));
}

static inline void trace_schib_ccw(pch_trc_record_type_t rt, pch_schib_t *schib, pch_ccw_t *ccw_addr, pch_ccw_t ccw) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trdata_ccw_addr_sid){
                        .ccw = ccw,
                        .addr = (uint32_t)ccw_addr,
                        .sid = get_sid(schib)
                }));
}

static inline void trace_schib_callback(pch_trc_record_type_t rt, pch_schib_t *schib, pch_intcode_t *ic) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trdata_intcode_scsw){
                        .intcode = *ic,
                        .scsw = schib->scsw,
                }));
}

static inline void trace_schib_scsw_cc(pch_trc_record_type_t rt, pch_schib_t *schib, pch_scsw_t *scsw, uint8_t cc) {
        PCH_CSS_TRACE_COND(rt, schib_is_traced(schib),
                ((struct pch_trdata_scsw_sid_cc){
                        .scsw = *scsw,
                        .sid = get_sid(schib),
                        .cc = cc
                }));
}

static inline void trace_chp_irq(pch_trc_record_type_t rt, pch_chp_t *chp, pch_dma_irq_index_t dmairqix, uint8_t tx_irq_state, uint8_t rx_irq_state) {
        PCH_CSS_TRACE_COND(rt,
                pch_chp_is_traced_irq(chp), ((struct pch_trdata_id_irq){
                        .id = pch_get_chpid(chp),
                        .dmairqix = dmairqix,
                        .tx_state = tx_irq_state << 4
                                | chp->channel.tx.mem_src_state,
                        .rx_state = rx_irq_state << 4
                                | chp->channel.rx.mem_dst_state
                }));
}

static inline void trace_chp_irq_progress(pch_trc_record_type_t rt, pch_chp_t *chp, bool rxcomplete, bool txcomplete, bool progress) {
        PCH_CSS_TRACE_COND(rt,
                pch_chp_is_traced_irq(chp), ((struct pch_trdata_id_byte){
                        .id = pch_get_chpid(chp),
                        .byte = ((uint8_t)rxcomplete << 2)
                                | ((uint8_t)txcomplete << 1)
                                | (uint8_t)progress
                }));
}
#endif
