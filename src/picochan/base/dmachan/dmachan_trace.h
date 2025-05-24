/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_DMACHAN_DMACHAN_TRACE_H
#define _PCH_DMACHAN_DMACHAN_TRACE_H

#include "trc/trace.h"

#define PCH_DMACHAN_LINK_TRACE(rt, l, data) \
        PCH_TRC_WRITE(l->bs, l->bs, (rt), (data))

struct trdata_dmachan {
        pch_dmaid_t     dmaid;
};

struct trdata_dmachan_memstate {
        pch_dmaid_t     dmaid;
        uint8_t         state;
};

struct trdata_dmachan_segment {
        uint32_t        addr;
        uint32_t        count;
        pch_dmaid_t     dmaid;
};

struct trdata_dmachan_segment_memstate {
        uint32_t        addr;
        uint32_t        count;
        pch_dmaid_t     dmaid;
        uint8_t         state;
};

static inline void trace_dmachan(pch_trc_record_type_t rt, dmachan_link_t *l) {
        PCH_DMACHAN_LINK_TRACE(rt, l, ((struct trdata_dmachan){
                .dmaid = l->dmaid
        }));
}

static inline void trace_dmachan_segment(pch_trc_record_type_t rt, dmachan_link_t *l, uint32_t addr, uint32_t count) {
        PCH_DMACHAN_LINK_TRACE(rt, l, ((struct trdata_dmachan_segment){
                .addr = addr,
                .count = count,
                .dmaid = l->dmaid
        }));
}

static inline void trace_dmachan_memstate(pch_trc_record_type_t rt, dmachan_link_t *l, uint8_t state) {
        PCH_DMACHAN_LINK_TRACE(rt, l,
                ((struct trdata_dmachan_memstate){
                        .dmaid = l->dmaid,
                        .state = state
                }));
}

static inline void trace_dmachan_segment_memstate(pch_trc_record_type_t rt, dmachan_link_t *l, uint32_t addr, uint32_t count, uint8_t state) {
        PCH_DMACHAN_LINK_TRACE(rt, l,
                ((struct trdata_dmachan_segment_memstate){
                        .addr = addr,
                        .count = count,
                        .dmaid = l->dmaid,
                        .state = state
                }));
}

#endif
