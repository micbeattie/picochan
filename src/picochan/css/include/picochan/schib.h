/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_SCHIB_H
#define _PCH_API_SCHIB_H

#include "picochan/ids.h"
#include "picochan/pmcw.h"
#include "picochan/scsw.h"

/*! \file picochan/schib.h
 *  \ingroup picochan_base
 *
 * \brief The Subchannel Information Block (SCHIB)
 */

/*! \brief The Model Dependent Area (MDA) of a schib.
 *
 * Although this structure is part of the schib, pch_schib_t,
 * and thus is visible to applications, the contents are for
 * internal use by the CSS.
 */
typedef struct pch_schib_mda {
        uint32_t        data_addr;
        uint16_t        devcount;
        pch_unit_addr_t prevua;
        pch_unit_addr_t nextua;
        pch_sid_t       prevsid;
        pch_sid_t       nextsid;
} pch_schib_mda_t;
static_assert(sizeof(pch_schib_mda_t) == 12,
        "pch_schib_mda_t should be 12 bytes");

/*! \brief pch_schib_t is the Subchannel Information Block (SCHIB)
 *  \ingroup picochan_base
 *
 *  The SCHIB is formed from the Path Management Control Word (PMCW),
 *  Subchannel Status Word (SCSW) and Model Dependent Area (MDA).
 *  Of these, the PMCW and SCSW are architected formats and the MDA
 *  format is an internal implementation detail of the CSS.
\verbatim
PMCW    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                            Intparm                            |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                     |T|E| ISC |      CUAddr   | UnitAddr      |
SCSW    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |               | CC|P|I|U|Z| |N|W|  FC |     AC      |   SC    |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                         CCW Address                           |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        | DEVS/ccwflags |     SCHS      |     Residual Count            |
MDA     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |                        data address                           |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |        reqcount/advcount      | prevua/ccwcmd |    nextua     |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        |           prevsid             |           nextsid             |
        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
\endverbatim
 * DEVS only needs to be valid when SC.StatusPending is set.
 * Otherwise, we use the field to hold the current ccwflags.
 */
typedef struct pch_schib {
        pch_pmcw_t      pmcw;
        pch_scsw_t      scsw;
        pch_schib_mda_t mda;
} pch_schib_t;
static_assert(sizeof(pch_schib_t) == 32,
        "pch_schib_t should be 32 bytes");

static inline bool schib_is_enabled(pch_schib_t *schib) {
        return schib->pmcw.flags & PCH_PMCW_ENABLED;
}

static inline bool schib_is_traced(pch_schib_t *schib) {
        return schib->pmcw.flags & PCH_PMCW_TRACED;
}

static inline bool schib_has_function_in_progress(pch_schib_t *schib) {
        const uint16_t mask = PCH_FC_START|PCH_FC_HALT|PCH_FC_CLEAR;
        return schib->scsw.ctrl_flags & mask;
}

static inline bool schib_is_status_pending(pch_schib_t *schib) {
        return schib->scsw.ctrl_flags & PCH_SC_PENDING;
}

#endif
