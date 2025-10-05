/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "css_internal.h"

static inline void raise_io_irq(void) {
        int16_t io_irqnum_opt = CSS.io_irqnum;
        if (io_irqnum_opt > 0)
                irq_set_pending((irq_num_t)io_irqnum_opt);
}

static inline void set_isc_status_bit(uint8_t iscnum) {
	CSS.isc_status_mask |= (uint8_t)(1 << iscnum);
}

static inline void unset_isc_status_bit(uint8_t iscnum) {
	CSS.isc_status_mask &= ~(uint8_t)(1 << iscnum);
}

static inline bool get_isc_status_bit(uint8_t iscnum) {
	return CSS.isc_status_mask & (uint8_t)(1 << iscnum);
}

static inline bool get_isc_enable_bit(uint8_t iscnum) {
	return CSS.isc_enable_mask & (uint8_t)(1 << iscnum);
}

static inline void set_isc_enable_bit(uint8_t iscnum) {
	CSS.isc_enable_mask |= (uint8_t)(1 << iscnum);
}

static inline void unset_isc_enable_bit(uint8_t iscnum) {
	CSS.isc_enable_mask &= ~(uint8_t)(1 << iscnum);
}

// Public API for ISC bits and masks follows

bool __time_critical_func(pch_css_is_isc_pending)(uint8_t iscnum) {
        valid_params_if(PCH_CSS, iscnum < PCH_NUM_ISCS);
        return get_isc_status_bit(iscnum);
}

bool __time_critical_func(pch_css_is_isc_enabled)(uint8_t iscnum) {
        valid_params_if(PCH_CSS, iscnum < PCH_NUM_ISCS);
        return get_isc_enable_bit(iscnum);
}

void __time_critical_func(pch_css_disable_isc)(uint8_t iscnum) {
        valid_params_if(PCH_CSS, iscnum < PCH_NUM_ISCS);
        unset_isc_enable_bit(iscnum);
}

void __time_critical_func(pch_css_disable_isc_mask)(uint8_t mask) {
        CSS.isc_enable_mask &= ~mask;
}

void __time_critical_func(pch_css_set_isc_enabled)(uint8_t iscnum, bool enabled) {
        valid_params_if(PCH_CSS, iscnum < PCH_NUM_ISCS);
        if (enabled) {
                set_isc_enable_bit(iscnum);
                if (get_isc_status_bit(iscnum))
                        raise_io_irq();
        } else {
                unset_isc_enable_bit(iscnum);
        }
}

void __time_critical_func(pch_css_enable_isc)(uint8_t iscnum) {
        valid_params_if(PCH_CSS, iscnum < PCH_NUM_ISCS);
        set_isc_enable_bit(iscnum);
        if (get_isc_status_bit(iscnum))
                raise_io_irq();
}

void __time_critical_func(pch_css_enable_isc_mask)(uint8_t mask) {
        uint8_t imask = PCH_NUM_ISCS - 1; // bits set for each existing ISC
        imask &= ~CSS.isc_enable_mask;    // valid but disabled bits
        imask &= mask;                    // the valid disabled bits to enable
        CSS.isc_enable_mask |= imask;     // set the new enable bits

        // are there newly-enabled ISC bits with non-empty lists?
        if (imask & CSS.isc_status_mask)
                raise_io_irq();
}

void __time_critical_func(pch_css_set_isc_enable_mask)(uint8_t mask) {
        // silently ignore bits for non-existent ISCs
        mask &= (uint8_t)(PCH_NUM_ISCS - 1);

        // bits we'll enable that aren't already:
        uint8_t imask = mask & ~CSS.isc_enable_mask;

        // update *all* mask (may disable some):
        CSS.isc_enable_mask = mask;

        // are there newly-enabled ISC bits with non-empty lists?
        if (imask & CSS.isc_status_mask)
                raise_io_irq();
}

// Following are internal to CSS

void __time_critical_func(remove_from_isc_dlist)(uint8_t iscnum, pch_sid_t sid) {
        valid_params_if(PCH_CSS, get_isc_status_bit(iscnum));
	schib_dlist_t *isc_dlist = get_isc_dlist(iscnum);
        
        remove_from_schib_dlist(isc_dlist, sid);
	if (*isc_dlist == -1)
                unset_isc_status_bit(iscnum);
}

pch_schib_t __time_critical_func(*pop_pending_schib_from_isc)(uint8_t iscnum) {
        valid_params_if(PCH_CSS, iscnum < PCH_NUM_ISCS);
	if (!get_isc_status_bit(iscnum))
		return NULL;

        schib_dlist_t *isc_dlist = get_isc_dlist(iscnum);
	pch_schib_t *schib = pop_schib_dlist(isc_dlist);
        assert(schib != NULL);

	if (*isc_dlist == -1)
		unset_isc_status_bit(iscnum); // List is now empty

	return schib;
}

// push_to_isc_dlist pushes schib onto its isc_dlist (indexed by
// schib->pmcw ISC field) and, if that isc_dlist_t was empty, sets the
// ISC's bit in isc_status_mask and, if that bit is also set in
// isc_enable_mask, raises the io_irq.
void __time_critical_func(push_to_isc_dlist)(pch_schib_t *schib) {
        uint8_t iscnum = pch_pmcw_isc(&schib->pmcw);
        schib_dlist_t *isc_dlist = get_isc_dlist(iscnum);
        pch_sid_t sid = get_sid(schib);
	bool was_empty = push_to_schib_dlist(isc_dlist, sid);

	if (!was_empty)
		return;

        if (!get_isc_status_bit(iscnum))
		set_isc_status_bit(iscnum);

        if (get_isc_enable_bit(iscnum))
                raise_io_irq();
}
