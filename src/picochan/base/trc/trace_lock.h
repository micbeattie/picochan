/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_TRC_TRACE_LOCK_H
#define _PCH_TRC_TRACE_LOCK_H

#include "hardware/sync.h"

static inline uint32_t trace_lock(void) {
        return save_and_disable_interrupts();
}

static inline void trace_unlock(uint32_t status) {
        restore_interrupts(status);
}

#endif
