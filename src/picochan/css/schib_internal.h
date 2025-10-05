/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CSS_SCHIB_INTERNAL_H
#define _PCH_CSS_SCHIB_INTERNAL_H

#include "picochan/schib.h"
#include "picochan/dev_status.h"
#include "picochan/ccw.h"

// get_stashed_ccw_flags is a CSS-internal function that fetches the
// CCW flags that we stash in the SCSW device status field during
// execution of a channel program. The SCSW device status field is
// only architected to be valid when Status Pending is set in the
// Status Control flags and we have to be careful only to stash
// CCW flags in this field when Status Pending is not set.
static inline pch_ccw_flags_t get_stashed_ccw_flags(pch_schib_t *schib) {
        return (pch_ccw_flags_t)schib->scsw.devs; // sic
}

#endif
