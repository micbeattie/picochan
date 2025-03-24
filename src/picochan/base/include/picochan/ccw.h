/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_H
#define _PCH_API_H

#include <stdint.h>
#include <assert.h>
#include "pico/platform/compiler.h"

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

// pch_ccw is an I/O Command Control Word (CCW). It is an architected
// 8-byte control block that must be 4-byte aligned. When
// marshalling/unmarshalling a CCW, unlike the original architected
// Format-1 CCW which was implicitly big-endian, the count and addr
// fields here are treated as native-endian and so will be
// little-endian on both ARM and RISC-V (in Pico configurations) and
// would also be so on x86, for example.
//
// CCW +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |      cmd      |     flags     |           count               |
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//     |                        data address                           |
//     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

typedef struct __aligned(4) pch_ccw {
	uint8_t         cmd;
	pch_ccw_flags_t flags;
	uint16_t        count;
	uint32_t        addr;
} pch_ccw_t;

static_assert(sizeof(pch_ccw_t) == 8, "architected pch_ccw_t is 8 bytes");

// Architected values of CCW commands
// TIC: Transfer In Channel
#define PCH_CCW_CMD_TIC 0x08

// Architected bit tests of CCW commands
static inline bool pch_is_ccw_cmd_write(uint8_t cmd) {
        return (cmd & 0x01) == 1;
}

#endif
