/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_TRC_TRACE_H
#define _PCH_TRC_TRACE_H

// PICO_CONFIG: PARAM_ASSERTIONS_ENABLED_PCH_TRC, Enable/disable assertions in the pch_trc module, type=bool, default=0, group=pch_trc
#ifndef PARAM_ASSERTIONS_ENABLED_PCH_TRC
#define PARAM_ASSERTIONS_ENABLED_PCH_TRC 0
#endif

#include <stddef.h>
#include <string.h>
#include "picochan/trc.h"
#include "picochan/trc_records.h"
#include "bufferset.h"

static_assert(sizeof(pch_trc_timestamp_t) == 6,
        "pch_trc_timestamp_t must be 6 bytes");

static_assert(sizeof(pch_trc_header_t) == 8,
        "pch_trc_header_t must be 8 bytes");

void *pch_trc_write_uncond(pch_trc_bufferset_t *bs, pch_trc_record_type_t rt, uint8_t data_size);

// pch_trc_write allocates and writes the header of a trace record
// with the current timestamp and record type rt and returns a pointer
// to the location where data_size bytes of associated trace should be
// written. It returns NULL (without writing any header or taking any
// other action) if no trace record should be written. This will be
// the case if tracing was disabled globally at compile time
// (PCH_CONFIG_ENABLE_TRACE was not defined or defined as 0) or if
// tracing has been disabled (perhaps temporarily) at runtime by
// setting pch_trc_enable to false or if the cond function argument is
// false.
static inline void *pch_trc_write(pch_trc_bufferset_t *bs, bool cond, pch_trc_record_type_t rt, uint8_t data_size) {
#if PCH_CONFIG_ENABLE_TRACE
        if (!bs->enable // per-bufferset runtime tracing flag not enabled
                || !cond) // per-function-call condition flag not enabled
                return NULL;

        return pch_trc_write_uncond(bs, rt, data_size);
#else
        (void)bs;
        (void)cond;
        (void)rt;
        (void)data_size;
#endif
        return NULL;
}

#define PCH_TRC_WRITE(bs, cond, rt, data) do { \
                size_t __data_size = sizeof (data); \
                void *__rec = pch_trc_write((bs), (cond), (rt), __data_size); \
                if (__rec) \
                        memcpy(__rec, &(data), __data_size); \
        } while (0)

bool pch_trc_set_enable(pch_trc_bufferset_t *bs, bool enable);

#endif
