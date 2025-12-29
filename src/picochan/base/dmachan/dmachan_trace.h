/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_DMACHAN_DMACHAN_TRACE_H
#define _PCH_DMACHAN_DMACHAN_TRACE_H

#include "trc/trace.h"
#include "picochan/trc_records.h"

#define PCH_DMACHAN_LINK_TRACE(rt, l, data) \
        PCH_TRC_WRITE((l)->bs, (l)->bs, (rt), (data))

static inline void trace_dmachan(pch_trc_record_type_t rt, dmachan_link_t *l) {
        PCH_DMACHAN_LINK_TRACE(rt, l, ((struct pch_trdata_dmachan){
                .dmaid = l->dmaid
        }));
}

// Values for pch_trdata_dmachan_byte for PCH_TRC_RT_DMACHAN_DST_RESET
#define DMACHAN_RESET_PROGRESSING       0
#define DMACHAN_RESET_COMPLETE          1
#define DMACHAN_RESET_BYPASSED          2
#define DMACHAN_RESET_INVALID           3

static inline void trace_dmachan_byte(pch_trc_record_type_t rt, dmachan_link_t *l, uint8_t byte) {
        PCH_DMACHAN_LINK_TRACE(rt, l, ((struct pch_trdata_dmachan_byte){
                .dmaid = l->dmaid,
                .byte = byte
        }));
}

static inline void trace_dmachan_segment(pch_trc_record_type_t rt, dmachan_link_t *l, uint32_t addr, uint32_t count) {
        PCH_DMACHAN_LINK_TRACE(rt, l, ((struct pch_trdata_dmachan_segment){
                .addr = addr,
                .count = count,
                .dmaid = l->dmaid
        }));
}

#endif
