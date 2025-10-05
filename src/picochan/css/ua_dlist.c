/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "css_internal.h"

pch_schib_t __time_critical_func(*remove_from_ua_dlist_unsafe)(ua_dlist_t *l, pch_chp_t *chp, pch_unit_addr_t ua) {
        pch_schib_t *schib = get_schib_by_chp(chp, ua);
	pch_unit_addr_t prev = schib->mda.prevua;
	pch_unit_addr_t next = schib->mda.nextua;
        pch_schib_t *prev_schib = get_schib_by_chp(chp, prev);
        pch_schib_t *next_schib = get_schib_by_chp(chp, next);
        prev_schib->mda.nextua = next;
        next_schib->mda.prevua = prev;

	if (*l == -1)
		panic("remove from empty ua_dlist");

	if ((pch_unit_addr_t)(*l) == ua) {
		if (next == ua)
			*l = -1;
		else
			*l = (ua_dlist_t)next;
	}

	return schib;
}

void __time_critical_func(push_ua_dlist_unsafe)(ua_dlist_t *l, pch_chp_t *chp, pch_schib_t *schib) {
        pch_unit_addr_t ua = schib->pmcw.unit_addr;
	if (*l == -1) {
		schib->mda.nextua = ua;
		schib->mda.prevua = ua;
		*l = (ua_dlist_t)ua;
		return;
	}

	pch_unit_addr_t first = (pch_unit_addr_t)*l;
        pch_schib_t *first_schib = get_schib_by_chp(chp, first);
	pch_unit_addr_t last = first_schib->mda.prevua;
        pch_schib_t *last_schib = get_schib_by_chp(chp, last);
	schib->mda.nextua = first;
	schib->mda.prevua = last;
	last_schib->mda.nextua = ua;
	first_schib->mda.prevua = ua;
}
