/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_TRC_BUFFERSET_H
#define _PCH_TRC_BUFFERSET_H

#include "hardware/irq.h"
#include "assert.h"
#include "trace_lock.h"

// pch_trc_init_bufferset initialises the bufferset by filling in
// the num_buffers, buffer_size and magic fields and zeroing out the
// other fields
void pch_trc_init_bufferset(pch_trc_bufferset_t *bs, uint32_t magic);

// pch_trc_init_buffer initialises buffer index n to buf.
static inline void pch_trc_init_buffer(pch_trc_bufferset_t *bs, uint n, void *buf) {
        valid_params_if(PCH_TRC, n < PCH_TRC_NUM_BUFFERS);
        valid_params_if(PCH_TRC, ((uint32_t)buf & 0x3) == 0);
        bs->buffers[n] = buf;
}

// pch_trc_init_contiguous_buffers initialises all buffers of bs
// to be pointers to the PCH_TRC_NUM_BUFFERS consecutive
// PCH_TRC_BUFFER_SIZE-bytes-sized buffers in the contiguous space
// in buf. buf must therefore be a pointer to at least
// PCH_TRC_NUM_BUFFERS*PCH_TRC_BUFFER_SIZE available bytes.
void pch_trc_init_all_buffers(pch_trc_bufferset_t *bs, void *buf);

// pch_trc_switch_to_next_buffer_unsafe is for internal use.
// The external API is pch_trc_switch_to_next_buffer which takes
// the trace_lock and then calls this with a 0 for position.
// Internally, this is used when allocating a slot for a new trace
// record (which has already taken trace_lock) and in that situation
// it is often called with a non-zero pos following the
// newly-allocated trace record.
static inline unsigned char *pch_trc_switch_to_next_buffer_unsafe(pch_trc_bufferset_t *bs, uint32_t pos) {
        bs->current_buffer_num = (bs->current_buffer_num + 1) % PCH_TRC_NUM_BUFFERS;
        bs->current_buffer_pos = pos;
        if (bs->irqnum > -1)
                irq_set_pending((irq_num_t)(bs->irqnum));

        return bs->buffers[bs->current_buffer_num];
}

// pch_trc_switch_to_next_buffer switches to the next trace buffer in
// the bufferset. If bs->irqnum is non-negative, that IRQ is raised.
// When the IRQ is raised, current_buffer_num has already been
// incremented (modulo PCH_TRC_NUM_BUFFERS) and a trace record may be
// in the process of writing to the new buffer. The IRQ handler will
// typically want to start copying or sending the contents of
// bs->buffers[bs->current_buffer_num-1] elsewhere and aim for
// completion before the trace records fill remaining buffers and
// wrap back around to overwrite that buffer.
static inline unsigned char *pch_trc_switch_to_next_buffer(pch_trc_bufferset_t *bs) {
        uint32_t status = trace_lock();
        unsigned char *rec = pch_trc_switch_to_next_buffer_unsafe(bs, 0);
        trace_unlock(status);
        return rec;
}

#endif
