/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_IDS_H
#define _PCH_API_IDS_H

#include <stdint.h>

// pch_sid_t is a subchannel id (SID) between 0 and PCH_NUM_SCHIBS-1
// (at most 65535).
typedef uint16_t pch_sid_t;

// pch_cunum_t is a control unit number between 0 and
// PCH_NUM_CSS_CUS-1 (at most 255) that identifies a control unit
// from the CSS side.
typedef uint8_t pch_cunum_t;

// pch_unit_addr_t is a unit address that identifies a device on the
// control unit side between 0 and NUM_DEVICES_PER_CU (at most 255).
typedef uint8_t pch_unit_addr_t;

typedef uint8_t pch_dmaid_t;

typedef uint8_t pch_dma_irq_index_t;

#endif
