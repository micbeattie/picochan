/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CUS_DEVIBS_LOCK_H
#define _PCH_CUS_DEVIBS_LOCK_H

#include "hardware/sync.h"

// devibs_lock() and devibs_unlock() protect manipulation of the
// linked lists of devibs 's with pending functions (i.e. API
// functions such as Start Subchannel). The device API uses a
// critical section protected by devibs_lock()/devibs_unlock() to
// add itself to the tx pending list headed by the devices' CU
// fields tx_head and tx_tail and linked via devib->next. The list
// is traversed and the pending packets sent (from the devib fields
// op and payload and using the devib's ua) whenever the CU's tx
// engine is free, driven by DMA completion on the tx channel.
// We assume the device API invocations and the CU itself run on the
// same core and so simply disable/restore (all) interrupts without
// needing to worry about cross-core locking.
static inline uint32_t devibs_lock(void) {
        return save_and_disable_interrupts();
}

static inline void devibs_unlock(uint32_t status) {
        restore_interrupts(status);
}

#endif
