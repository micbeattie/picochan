/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CSS_PMCW_H
#define _PCH_CSS_PMCW_H

#include <stdbool.h>
#include "picochan/ids.h"

/*! \file picochan/pmcw.h
 *  \ingroup picochan_css
 *
 * \brief The Path Management Control World (PMCW)
 */

/*! pch_pmcw_t is the Path Management Control World (PMCW)
 *
 * This is an architected part of the schib. It contains
 * * the addressing information for the CSS to communicate with the
 * device on its CU (see below)
 * * An Interruption Parameter (intparm) - a 32-bit value which is
 * not modified by the CSS and can be used by the application for
 * any purpose
 * * An Interrupt Service Class (ISC) so that groups of subchannels
 * can be masked/unmasked together from delivering I/O interruptions
 * * The flag which indicates that the subchannel is enabled and can
 * thus run channel programs
 * * A "trace" flag to indicate whether events for this subchannel
 * can cause trace records to be written
 *
 * Although for a mainframe channel subsystem, the addressing
 * information in the PMCW contains 8 x 8-bit channel path id numbers
 * referencing one or more channels that can reach the control unit,
 * for picochan, the addressing information is simply a single channel
 * path id (CHPID) and and the unit address of the device on the
 * single remote CU to which it is connected.
 *
 * The addressing information (CHPID and UnitAddr) must be set by the
 * application (by using pch_chp_alloc) before the channel is started.
\verbatim
PMCW    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |               Interruption Parameter (Intparm)                |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                     |T|E| ISC |      CHPID    | UnitAddr      |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
\endverbatim
 */
typedef struct pch_pmcw {
        uint32_t        intparm;
        uint16_t        flags;
        pch_chpid_t     chpid;
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
void pch_css_set_isc_enabled(uint8_t iscnum, bool enabled);
void pch_css_disable_isc(uint8_t iscnum);
void pch_css_disable_isc_mask(uint8_t mask);
void pch_css_enable_isc(uint8_t iscnum);
void pch_css_enable_isc_mask(uint8_t mask);
void pch_css_set_isc_enable_mask(uint8_t mask);

#endif
