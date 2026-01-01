/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_DMACHAN_PIOCHAN_H
#define _PCH_DMACHAN_PIOCHAN_H

#include "hardware/pio.h"

static inline void pch_pio_set_irqn_irqflag_enabled(PIO pio, uint irq_index, uint ir
qflag, bool enabled) {
        const pio_interrupt_source_t source = irqflag + PIO_INTR_SM0_LSB;
        pio_set_irqn_source_enabled(pio, irq_index, source, enabled);
}

#endif
