/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "picochan/dmachan.h"
#include "dmachan_internal.h"

// mem_peer_spin_lock must be initialised with pch_memchan_init
spin_lock_t *dmachan_mem_peer_spin_lock;

void dmachan_panic_unless_memchan_initialised() {
        if (!dmachan_mem_peer_spin_lock)
                panic("pch_memchan_init not called");
}

// pch_memchan_init must be called to initialise mem_peer_spin_lock
// before configuring any memchan CU. If it is not initialised then
// any attempt to configure a memcu from either CSS or CUS side
// will panic at runtime to avoid any mysterious race conditions.
void pch_memchan_init(void) {
        if (dmachan_mem_peer_spin_lock)
                panic("dmachan_mem_peer_spin_lock already initialised");

        int n = spin_lock_claim_unused(true);
        dmachan_mem_peer_spin_lock = spin_lock_init(n);
}
