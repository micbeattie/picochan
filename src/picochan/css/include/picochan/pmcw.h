/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CSS_PMCW_H
#define _PCH_CSS_PMCW_H

#include <stdbool.h>
#include "picochan/ids.h"

// pch_pmcw_t is the Path Management Control World (PMCW).
// PMCW    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |               Interruption Parameter (Intparm)                |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |                     |T|E| ISC |      CUAddr   | UnitAddr      |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct pch_pmcw {
        uint32_t        intparm;
        uint16_t        flags;
        pch_cunum_t     cu_number;
        pch_unit_addr_t unit_addr;
} pch_pmcw_t;

// PCH_PMCW_SCH_MODIFY_MASK are the bits of the PMCW flags
// which can be set with the Modify Subchannel function
#define PCH_PMCW_SCH_MODIFY_MASK 0x001f

// ISC: Interrupt Service Class - the low 3 bits of the PMCW.
// We define PCH_PMCW_ISC_LSB as the shift count to get the ISC bits
// in case we ever want to move them, even though it's currently 0.
#define PCH_PMCW_ISC_BITS       0x07
#define PCH_PMCW_ISC_LSB        0
#define PCH_PMCW_ENABLED        0x08
#define PCH_PMCW_TRACED         0x10

static inline uint8_t pch_pmcw_isc(pch_pmcw_t *pmcw) {
        return (pmcw->flags & PCH_PMCW_ISC_BITS) >> PCH_PMCW_ISC_LSB;
}

bool pch_css_is_isc_enabled(uint8_t iscnum);
void pch_css_disable_isc(uint8_t iscnum);
void pch_css_disable_isc_mask(uint8_t mask);
void pch_css_enable_isc(uint8_t iscnum);
void pch_css_enable_isc_mask(uint8_t mask);
void pch_css_set_isc_enable_mask(uint8_t mask);

#endif
