/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"

pch_schib_t __time_critical_func(*pop_ua_slist_unsafe)(ua_slist_t *l, css_cu_t *cu) {
	int16_t head = l->head;
	if (head == -1)
		return NULL;

        pch_unit_addr_t ua = (pch_unit_addr_t)head;
        pch_schib_t *schib = get_schib_by_cu(cu, ua);
        pch_unit_addr_t next = schib->mda.nextua;
	if (next == ua) {
                assert(l->tail == (int16_t)ua);
		reset_ua_slist(l);
	} else {
		// mark popped schib as no longer in a list:
		schib->mda.nextua = ua;
		l->head = (uint16_t)next;
	}

	return schib;
}

bool __time_critical_func(push_ua_slist_unsafe)(ua_slist_t *l, css_cu_t *cu, pch_sid_t sid) {
        bool was_empty = false;
        pch_schib_t *schib = get_schib(sid);
        pch_unit_addr_t ua = schib->pmcw.unit_addr;
	int16_t tail = l->tail;
	if (tail == -1) {
                assert(l->head == -1);
		l->head = (int16_t)ua;
		was_empty = true;
	} else {
                pch_unit_addr_t tail_ua = (pch_unit_addr_t)tail;
                pch_schib_t *tail_schib = get_schib_by_cu(cu, tail_ua);
                assert(tail_schib->mda.nextua == tail_ua);
		tail_schib->mda.nextua = ua;
	}

	l->tail = (int16_t)ua;
	return was_empty;
}
