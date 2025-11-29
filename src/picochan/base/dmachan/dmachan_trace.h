/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_DMACHAN_DMACHAN_TRACE_H
#define _PCH_DMACHAN_DMACHAN_TRACE_H

#include "trc/trace.h"
#include "picochan/trc_records.h"

#ifndef PCH_DMACHAN_MEMCHAN_DEBUG_ENABLED
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
#define PCH_DMACHAN_MEMCHAN_DEBUG_ENABLED true
#else
#define PCH_DMACHAN_MEMCHAN_DEBUG_ENABLED false
#endif
#endif

#define PCH_DMACHAN_LINK_TRACE(rt, l, data) \
        PCH_TRC_WRITE(l->bs, l->bs, (rt), (data))

#define PCH_DMACHAN_LINK_MEMCHAN_DEBUG_TRACE(rt, l, data) \
        PCH_TRC_WRITE(l->bs, PCH_DMACHAN_MEMCHAN_DEBUG_ENABLED && l->bs, (rt), (data))

static inline void trace_dmachan(pch_trc_record_type_t rt, dmachan_link_t *l) {
        PCH_DMACHAN_LINK_TRACE(rt, l, ((struct pch_trdata_dmachan){
                .dmaid = l->dmaid
        }));
}

static inline void trace_dmachan_segment(pch_trc_record_type_t rt, dmachan_link_t *l, uint32_t addr, uint32_t count) {
        PCH_DMACHAN_LINK_TRACE(rt, l, ((struct pch_trdata_dmachan_segment){
                .addr = addr,
                .count = count,
                .dmaid = l->dmaid
        }));
}

static inline void trace_dmachan_memstate(pch_trc_record_type_t rt, dmachan_link_t *l, uint8_t state) {
        PCH_DMACHAN_LINK_TRACE(rt, l,
                ((struct pch_trdata_dmachan_memstate){
                        .dmaid = l->dmaid,
                        .state = state
                }));
}

static inline void trace_dmachan_segment_memstate(pch_trc_record_type_t rt, dmachan_link_t *l, uint32_t addr, uint32_t count, uint8_t state) {
        PCH_DMACHAN_LINK_TRACE(rt, l,
                ((struct pch_trdata_dmachan_segment_memstate){
                        .addr = addr,
                        .count = count,
                        .dmaid = l->dmaid,
                        .state = state
                }));
}

static inline void trace_dmachan_cmd(pch_trc_record_type_t rt, dmachan_link_t *l) {
        PCH_DMACHAN_LINK_MEMCHAN_DEBUG_TRACE(rt, l,
                ((struct pch_trdata_dmachan_cmd){
                        .cmd = l->cmd.raw,
                        .seqnum = dmachan_link_seqnum(l),
                        .dmaid = l->dmaid
                }));
}

#endif
