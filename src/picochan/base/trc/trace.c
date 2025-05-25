/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "pico/time.h"
#include "assert.h"
#include "trace.h"
#include "trace_lock.h"

uint32_t pch_trc_buffer_size = PCH_TRC_BUFFER_SIZE;
uint32_t pch_trc_num_buffers = PCH_TRC_NUM_BUFFERS;

static inline void pch_trc_write_current_timestamp(pch_trc_timestamp_t *tp) {
        uint64_t us = to_us_since_boot(get_absolute_time());
        pch_trc_write_timestamp(tp, us);
}

void pch_trc_init_bufferset(pch_trc_bufferset_t *bs, uint32_t magic) {
        memset(bs, 0, sizeof(*bs));
        bs->magic = magic;
        bs->buffer_size = PCH_TRC_BUFFER_SIZE;
        bs->num_buffers = PCH_TRC_NUM_BUFFERS;
}

void pch_trc_init_all_buffers(pch_trc_bufferset_t *bs, void *buf) {
        unsigned char *p = (unsigned char *)buf;

        for (uint i = 0; i < PCH_TRC_NUM_BUFFERS; i++) {
                pch_trc_init_buffer(bs, i, p);
                p += PCH_TRC_BUFFER_SIZE;
        }
}

// alloc_trace_slot returns a pointer to where the next trace record
// can be written. There is room at that location for a header
// (pch_trc_header_t) followed by data_size bytes of trace data.
// Before returning, the current_buffer_num and current_buffer_pos
// are updated ready for the next record (the one after the slot being
// allocated here) so if no record is written to the returned slot
// then there will be a gap containing stale data from whatever was
// in the buffer beforehand. This function takes trace_lock while
// checking and changing bufferset current_buffer_num and
// current_buffer_pos fields so is as safe for calling concurrently
// as trace_lock allows. Currently, trace_lock only disables
// interrupts so concurrent use on the same core is safe but not on
// different cores (for which we'd need to use a hardware spinlock).
static pch_trc_header_t *alloc_trace_slot(pch_trc_bufferset_t *bs, uint8_t data_size) {
        valid_params_if(PCH_TRC,
                ((uint32_t)data_size) + sizeof(pch_trc_header_t) <= 252);
        uint32_t size = sizeof(pch_trc_header_t) + data_size;
        size = (size + 3) & ~3; // round up to 4-byte alignment
        unsigned char *rec;

        uint32_t status = trace_lock();

        unsigned char *buf = bs->buffers[bs->current_buffer_num];
        assert(buf);
        rec = &buf[bs->current_buffer_pos];

        uint32_t endpos = bs->current_buffer_pos + size;
        if (endpos <= PCH_TRC_BUFFER_SIZE)
                bs->current_buffer_pos = endpos;
        else
                rec = pch_trc_switch_to_next_buffer_unsafe(bs, size);

        trace_unlock(status);
        return (pch_trc_header_t*)__builtin_assume_aligned(rec, 4);
}

// pch_trc_write_uncond allocates a trace record slot, writes a header
// to it (pch_trc_header_t) with the current timestamp, record type rt
// and a size field corresponding to a record with data_size of the
// associated trace data. It returns a pointer to the location where
// those data_size bytes of trace data can be written. If no data is
// subsequently written there, the trace record will have a header
// with valid details to chain to subsequent records but the
// associated trace data bytes will contain whatever stale data was in
// the buffer beforehand.
void __time_critical_func(*pch_trc_write_uncond)(pch_trc_bufferset_t *bs, pch_trc_record_type_t rt, uint8_t data_size) {
        pch_trc_header_t *h = alloc_trace_slot(bs, data_size);
        pch_trc_write_current_timestamp(&h->timestamp);
        h->rec_type = rt;
        h->size = (sizeof(pch_trc_header_t) + data_size + 3) & ~3;
        return h + 1;
}

bool pch_trc_set_enable(pch_trc_bufferset_t *bs, bool enable) {
        bool old_enable = bs->enable;
        if (old_enable == enable)
                return old_enable; // nothing to do

        bs->enable = enable;

        PCH_TRC_WRITE(bs, true, PCH_TRC_RT_TRC_ENABLE,
                ((struct pch_trdata_byte){enable}));
        return old_enable;
}
