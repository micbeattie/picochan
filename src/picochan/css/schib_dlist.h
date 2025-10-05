/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CSS_SCHIB_DLIST_H
#define _PCH_CSS_SCHIB_DLIST_H

// schib_dlist_t is a doubly linked (by sid) list of schibs
typedef int32_t schib_dlist_t;

pch_schib_t *remove_from_schib_dlist_unsafe(schib_dlist_t *l, pch_sid_t sid);
bool push_to_schib_dlist_unsafe(schib_dlist_t *l, pch_sid_t sid);

static inline pch_schib_t *remove_from_schib_dlist(schib_dlist_t *l, pch_sid_t sid) {
        uint32_t status = schibs_lock();
        pch_schib_t *schib = remove_from_schib_dlist_unsafe(l, sid);
        schibs_unlock(status);
        return schib;
}

static inline pch_schib_t *pop_schib_dlist_unsafe(schib_dlist_t *l) {
        if (*l == -1)
                return NULL;

        return remove_from_schib_dlist_unsafe(l, (pch_sid_t)*l);
}

static inline pch_schib_t *pop_schib_dlist(schib_dlist_t *l) {
        uint32_t status = schibs_lock();
        pch_schib_t *schib = pop_schib_dlist_unsafe(l);
        schibs_unlock(status);
        return schib;
}

static inline bool push_to_schib_dlist(schib_dlist_t *l, pch_sid_t sid) {
        uint32_t status = schibs_lock();
        bool was_empty = push_to_schib_dlist_unsafe(l, sid);
        schibs_unlock(status);
        return was_empty;
}

#endif
