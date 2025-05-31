#ifndef _PCH_API_INTCODE_H
#define _PCH_API_INTCODE_H

#include "picochan/ids.h"

// An I/O interruption code is returned from pch_test_pending_interruption.
// (The original expansion of the acronym SID is
// Subsystem-Identification Word which is 32 bits and includes some bits of
// data beyond just the subchannel number. For Picochan we only use the
// 16-bit subchannel number so calling this the SID is more approriate.)
//
// pch_intcode_t
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |               Interruption Parameter (Intparm)                |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |  Subchannel ID (SID)          |      ISC      |           |cc |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// The cc is the condition code which, for a return from
// pch_test_pending_interruption, only uses two values: 0 means there was
// no interrupt pending and the rest of the pch_intcode_t is meaningless;
// 1 means an interrupt was pending and its information has been returned.
typedef struct pch_intcode {
        uint32_t        intparm;
        pch_sid_t       sid;
        uint8_t         flags;
        uint8_t         cc;
} pch_intcode_t;
static_assert(sizeof(pch_intcode_t) == 8,
        "architected pch_intcode_t is 8 bytes");

#endif
