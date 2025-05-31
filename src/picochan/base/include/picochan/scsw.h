/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_SCSW_H
#define _PCH_API_SCSW_H

#include <stdint.h>
#include <assert.h>

#define PCH_SF_CC_MASK  0xc0
#define PCH_SF_CC_SHIFT 6
#define PCH_SF_P        0x20
#define PCH_SF_I        0x10
#define PCH_SF_U        0x08
#define PCH_SF_Z        0x04
#define PCH_SF_UNUSED   0x02
#define PCH_SF_N        0x01

// uint16_t of SCSW Control flags W,FC,AC,SC (Function, Activity, Status)
#define PCH_SCSW_CCW_WRITE       0x8000

#define PCH_FC_MASK              0x7000
#define PCH_FC_START             0x4000
#define PCH_FC_HALT              0x2000
#define PCH_FC_CLEAR             0x1000

#define PCH_AC_MASK              0x0fe0
#define PCH_AC_RESUME_PENDING    0x0800
#define PCH_AC_START_PENDING     0x0400
#define PCH_AC_HALT_PENDING      0x0200
#define PCH_AC_CLEAR_PENDING     0x0100
#define PCH_AC_SUBCHANNEL_ACTIVE 0x0080
#define PCH_AC_DEVICE_ACTIVE     0x0040
#define PCH_AC_SUSPENDED         0x0020

#define PCH_SC_MASK              0x001f
#define PCH_SC_ALERT             0x0010
#define PCH_SC_INTERMEDIATE      0x0008
#define PCH_SC_PRIMARY           0x0004
#define PCH_SC_SECONDARY         0x0002
#define PCH_SC_PENDING           0x0001

// uint8_t of Subchannel Status (SCHS) flags
#define PCH_SCHS_PROGRAM_CONTROLLED_INTERRUPTION 0x80
#define PCH_SCHS_INCORRECT_LENGTH                0x40
#define PCH_SCHS_PROGRAM_CHECK                   0x20
#define PCH_SCHS_PROTECTION_CHECK                0x10
#define PCH_SCHS_CHANNEL_DATA_CHECK              0x08
#define PCH_SCHS_CHANNEL_CONTROL_CHECK           0x04
#define PCH_SCHS_INTERFACE_CONTROL_CHECK         0x02
#define PCH_SCHS_CHAINING_CHECK                  0x01

// pch_scsw_t is the Subchannel Status Word (SCSW) which must be
// 4-byte aligned. When marshalling/unmarshalling an SCSW, unlike the
// original architected SCSW which was implicitly big-endian, the
// ccw_addr and count fields here are treated as native-endian and so
// will be little-endian on both ARM and RISC-V (in Pico
// configurations) and would also be so on x86, for example. The flags
// fields are slightly rearranged from their original architected
// positions and some have been dropped and one or two added.
//
// SCSW    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |               | CC|P|I|U|Z| |N|W|  FC |     AC      |   SC    |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |                         CCW Address                           |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |      DEVS     |     SCHS      |     Residual Count            |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct __attribute__((aligned(4))) pch_scsw {
    uint8_t     __unused_flags;
    uint8_t     user_flags;
    uint16_t    ctrl_flags;
    uint32_t    ccw_addr;
    uint8_t     devs;
    uint8_t     schs;
    uint16_t    count;
} pch_scsw_t;
static_assert(sizeof(pch_scsw_t) == 12, "architected pch_scsw_t is 12 bytes");

#endif
