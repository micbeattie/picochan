/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "picochan/dmachan.h"
#include "mem_peer.h"

// mem_peer_spin_lock must be initialised with init_mem_peer_lock
spin_lock_t *mem_peer_spin_lock;

void init_mem_peer_lock(void) {
        int n = spin_lock_claim_unused(true);
        mem_peer_spin_lock = spin_lock_init(n);
}
