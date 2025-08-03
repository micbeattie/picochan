/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CSS_SCHIBS_LOCK_H
#define _PCH_CSS_SCHIBS_LOCK_H

#include "hardware/sync.h"

// schibs_lock() and schibs_unlock() protect manipulation of the
// linked lists of schib's with pending functions (i.e. API
// functions such as Start Subchannel). The user API uses a
// critical section protected by schibs_lock()/schibs_unlock() to
// update the Function Control flags in the target schib with the
// request, add itself to the ua_func_dlist headed by the channel
// responsible for the subchannel (linked via mda.prevua/nextua) and
// ping the CSS with raise_func_irq.
// At the moment, we assume the user API invocations and the CSS
// itself run on the same core and so the ping is raising a
// (non-hardware-connected) IRQ and lock/unlock is a simple
// disable/restore of (all) interrupts. If we want to separate out
// the user invocations onto a different core from the CSS itself
// (and there's no inherent problem with that since the CSS runs
// entirely asynchronously and can cope with that) then we can
// change the ping to be a doorbell interrupt to the other core
// and change the lock/unlock to be a (hardware) spinlock plus the
// disable/restore of interrupts.

static inline uint32_t schibs_lock(void) {
        return save_and_disable_interrupts();
}

static inline void schibs_unlock(uint32_t status) {
        restore_interrupts(status);
}

#endif
