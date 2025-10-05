/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "css_internal.h"

pch_schib_t __time_critical_func(*remove_from_schib_dlist_unsafe)(schib_dlist_t *l, pch_sid_t sid) {
        pch_schib_t *schib = get_schib(sid);
        pch_sid_t prev = schib->mda.prevsid;
        pch_sid_t next = schib->mda.nextsid;
        pch_schib_t *prev_schib = get_schib(prev);
        pch_schib_t *next_schib = get_schib(next);
        prev_schib->mda.nextsid = next;
        next_schib->mda.prevsid = prev;

	if (*l == -1)
		panic("remove from empty schib dlist");

	if ((pch_sid_t)(*l) == sid) {
		if (next == sid)
			*l = -1;
		else
			*l = (schib_dlist_t)next;
	}

	return schib;
}

bool __time_critical_func(push_to_schib_dlist_unsafe)(schib_dlist_t *l, pch_sid_t sid) {
        pch_schib_t *schib = get_schib(sid);
	if (*l == -1) {
		schib->mda.nextsid = sid;
		schib->mda.prevsid = sid;
		*l = (schib_dlist_t)sid;
		return true;
	}

	pch_sid_t first = (pch_sid_t)(*l);
        pch_schib_t *first_schib = get_schib(first);
        pch_sid_t last = first_schib->mda.prevsid;
        pch_schib_t *last_schib = get_schib(last);
        schib->mda.nextsid = first;
        schib->mda.prevsid = last;
	last_schib->mda.nextsid = sid;
	first_schib->mda.prevsid = sid;
	return false;
}
