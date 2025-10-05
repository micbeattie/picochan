/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_API_TRC_H
#define _PCH_API_TRC_H

#include "picochan/ids.h"

/*! \file picochan/trc.h
 *  \defgroup internal_trc internal_trc
 *
 * \brief Internal. Tracing subsystem used by both CSS and CU
 */

/*! \brief an opaque timestamp of a 48-bit number of microseconds since boot.
 *  \ingroup internal_trc
 *
 * The actual value is held as three consecutive 16-bit chunks
 * (forming a little-endian encoding of the whole value) but the
 * intended way of accessing the value is with
 * pch_trc_timestamp_to_us().
 */
typedef struct pch_trc_timestamp {
        uint16_t low;
        uint16_t mid;
        uint16_t high;
} pch_trc_timestamp_t;

static inline uint64_t pch_trc_timestamp_to_us(pch_trc_timestamp_t t) {
        return (((uint64_t) t.high) << 32)
                + (((uint64_t) t.mid) << 16)
                + (((uint64_t) t.low));
}

static inline void pch_trc_write_timestamp(pch_trc_timestamp_t *tp, uint64_t us) {
        uint16_t *hp = (uint16_t*)tp;
        hp[0] = (uint16_t)us; // low 16 bits: t.low
        hp[1] = (uint16_t)(us >> 16); // middle 16 bits: t.mid
        hp[2] = (uint16_t)(us >> 32); // low 16 bits of top 32: t.high
}

// The following macro definition nastiness allows a host-based
// trace dump program to redefine these macros to build a list
// of the record type names along with the enum values themselves.
#define PCH_TRC_RT(rt) PCH_TRC_RT_ ## rt

typedef enum __attribute__((__packed__)) pch_trc_record_type {
#include "picochan/trc_record_types.h"
} pch_trc_record_type_t;

typedef struct __attribute__((__packed__,__aligned__(2))) pch_trc_header {
        pch_trc_timestamp_t     timestamp;
        uint8_t                 size; // includes header and following data
        pch_trc_record_type_t   rec_type;
} pch_trc_header_t;

/*! \brief Whether any tracing code should be compiled at at..
 *  \ingroup internal_trc
 *
 * Set to a compile-time non-empty non-zero constant to enable.
 * Default 0.
 */
#ifndef PCH_CONFIG_ENABLE_TRACE
#define PCH_CONFIG_ENABLE_TRACE 0
#endif

#if PCH_CONFIG_ENABLE_TRACE

#ifndef PCH_TRC_BUFFER_SIZE
/*! \brief The size in bytes of each of the PCH_TRC_NUM_BUFFERS trace buffers
 *  \ingroup internal_trc
 *
 * Only relevant when PCH_CONFIG_ENABLE_TRACE is enabled.
 * Default 1024.
 */
#define PCH_TRC_BUFFER_SIZE 1024
#endif

#ifndef PCH_TRC_NUM_BUFFERS
/*! \brief The number of buffers each of size PCH_TRC_BUFFER_SIZE to hold tracing records
 *  \ingroup internal_trc
 *
 * Only relevant when PCH_CONFIG_ENABLE_TRACE is enabled.
 * Default 2.
 */
#define PCH_TRC_NUM_BUFFERS 2
#endif

/*! \brief pch_trc_buffer_size is initialised to PCH_TRC_BUFFER_SIZE so that
 * its value is visible in memory
 *  \ingroup internal_trc
 */
extern uint32_t pch_trc_buffer_size;

/*! \brief pch_trc_num_buffers is initialised to PCH_TRC_NUM_BUFFERS so that
 * its value is visible in memory
 *  \ingroup internal_trc
 */
extern uint32_t pch_trc_num_buffers;

#else
// PCH_CONFIG_ENABLE_TRACE is not defined

#ifndef PCH_TRC_BUFFER_SIZE
#define PCH_TRC_BUFFER_SIZE 0
#endif

#ifndef PCH_TRC_NUM_BUFFERS
#define PCH_TRC_NUM_BUFFERS 1
#endif

#endif
// end of PCH_CONFIG_ENABLE_TRACE section

/*! \brief set of buffers and metadata for a subsystem to use tracing
 *  \ingroup internal_trc
 *
 * This struct holds an array of PCH_TRC_NUM_BUFFERS buffers, each
 * which must be of size PCH_TRC_BUFFER_SIZE.
 *
 * When compile-time trace support is enabled (PCH_CONFIG_ENABLE_TRACE
 * is defined to be non-zero), PCH_TRC_NUM_BUFFERS is the number of
 * trace buffers in a bufferset. These buffers form a ring - once the
 * current buffer is full, the current buffer moves onto the next in
 * the ring and, optionally, an interrupt is generated so that the
 * previous buffer can be archived elsewhere before the ring wraps.
 *
 * When compile-time trace support is not enabled, PCH_TRC_NUM_BUFFERS
 * is defined as 0 so this struct can be instantiated but not used.
 */
typedef struct pch_trc_bufferset {
        //! the index in buffers of the current buffer being appended to
        uint32_t        current_buffer_num;

        //! \brief the byte offset in the current buffer where the
        //! next trace record will be written.
        uint32_t        current_buffer_pos;

        //! \brief the irq_num_t of an IRQ or -1
        //!
        //! When not -1, raised when pch_trc_switch_to_next_buffer is
        //! called either by explicit invocation or when writing a
        //! trace record skips to the next trace buffer because the
        //! current buffer is full.
        int16_t         irqnum;

        //! \brief the bufferset enablement flag for tracing.
        //! When false, no trace records will be written and all of the
        //! buffer arrays, pointers and indexes above are ignored.
        bool            enable;

        //! \brief subsystem-specific magic number for identifying
        //! dumped trace buffers
        uint32_t        magic;
        uint32_t        buffer_size;
        uint16_t        num_buffers;
        //! \brief the array of trace buffers.
        //!
        //! It is treated as a single ring buffer of trace records.
        //! Each trace record is of the form of an 8-byte header
        //! (pch_trc_header_t) followed by a number of bytes of
        //! associated trace data. The total size of header plus its
        //! following associated data is in the size field of the
        //! header.
        void            *buffers[PCH_TRC_NUM_BUFFERS];
} pch_trc_bufferset_t;

#endif
