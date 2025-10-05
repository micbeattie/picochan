/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_API_CCW_H
#define _PCH_API_CCW_H

#include <stdint.h>
#include <assert.h>

/*! \file picochan/ccw.h
 *  \ingroup picochan_base
 *
 * \brief Channel-Command Word (CCW)
 */

/*! \brief the flags of a CCW
 *  \ingroup picochan_base
 */
typedef uint8_t pch_ccw_flags_t;

// pch_ccw_flags_t: CCW flags
// CD: Chain Data
#define PCH_CCW_FLAG_CD         0x80
// CC: Chain Command
#define PCH_CCW_FLAG_CC         0x40
// SLI: Suppress Length Indication
#define PCH_CCW_FLAG_SLI        0x20
// SKP: Skip/Discard data
#define PCH_CCW_FLAG_SKP        0x10
// PCI: Program Controlled Interruption
#define PCH_CCW_FLAG_PCI        0x08
// IDA: Indirect Data Address (not used in Picochan)
#define PCH_CCW_FLAG_IDA        0x04
// S: Suspend
#define PCH_CCW_FLAG_S          0x02
// MIDA: Modified Indirect Data Address (not used in Picochan)
#define PCH_CCW_FLAG_MIDA       0x01

/*! \brief I/O Channel-Command Word (CCW)
 *  \ingroup picochan_base
 *
 * pch_ccw_t is an architected 8-byte control block that must be
 * 4-byte aligned. When marshalling/unmarshalling a CCW, unlike the
 * original architected Format-1 CCW which was implicitly big-endian,
 * the count and addr fields here are treated as native-endian and so
 * will be little-endian on both ARM and RISC-V (in Pico
 * configurations) and would also be so on x86, for example.
 *
\verbatim
CCW +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      cmd      |     flags     |           count               |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        data address                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
\endverbatim
 */
typedef struct __attribute__((aligned(4))) pch_ccw {
	uint8_t         cmd;
	pch_ccw_flags_t flags;
	uint16_t        count;
	uint32_t        addr;
} pch_ccw_t;

static_assert(sizeof(pch_ccw_t) == 8, "architected pch_ccw_t is 8 bytes");

// Architected values of CCW commands.
// These do not match those for traditional CSS and we only divide
// into "Read/Write" via the low bit instead of into Control/Read/
// ReadBackward/Sense/Test/Write via various low-bit groups.

#define PCH_CCW_CMD_FIRST_RESERVED 0xf0
// WRITE
#define PCH_CCW_CMD_WRITE       0x01
// READ
#define PCH_CCW_CMD_READ        0x02
// TIC: Transfer In Channel
#define PCH_CCW_CMD_TIC         0xf0
// SENSE: Read Sense data from devib
#define PCH_CCW_CMD_SENSE       0xf2

// Architected bit tests of CCW commands
static inline bool pch_is_ccw_cmd_write(uint8_t cmd) {
        return (cmd & 0x01) == 1;
}

#endif
