/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_IDS_H
#define _PCH_API_IDS_H

#include <stdint.h>

/*! \file picochan/ids.h
 *  \defgroup picochan_base picochan_base
 *
 * \brief The basic types used by picochan throughout both CSS and CU
 */

/*! \brief a subchannel id (SID) between 0 and PCH_NUM_SCHIBS-1 (at most 65535)
 *  \ingroup picochan_base
 */
typedef uint16_t pch_sid_t;

/*! \brief a control unit number between 0 and PCH_NUM_CSS_CUS-1
 * (at most 255) that identifies a control unit
 *  \ingroup picochan_base
 *
 * Both CSS and CU use a value of this type to index into their
 * respective arrays of CU structures. Currently, it is expected that
 * the numbers match meaning that even for a CSS with channels to two
 * independent CUs, the CUs must use different CU numbers.
 */
typedef uint8_t pch_cunum_t;

/*! \brief a unit address that identifies a device on a given CU on the control unit side
 *  \ingroup picochan_base
 *
 *  Must be between 0 and NUM_DEVICES_PER_CU (at most 255).
 */
typedef uint8_t pch_unit_addr_t;

/*! \brief a DMA id used by CSS or CU
 *  \ingroup picochan_base
 *
 *  Must be between 0 and the number of DMA channels on the platform.
 *  Pico SDK uses the uint type for DMA channel id arguments but
 *  picochan uses pch_dmaid_t type in its API and also for storing
 *  them in a single byte instead of four.
 */
typedef uint8_t pch_dmaid_t;

/*! \brief a DMA IRQ index
 *  \ingroup picochan_base
 *
 *  Must be between 0 and the number of DMA IRQs on the platform
 *  (e.g. 2 for RP2040 and 4 for RP2350).
 *  Pico SDK uses the uint type for DMA channel id arguments but
 *  picochan uses pch_dmaid_t type in its API and also for storing
 *  them so it can use a single byte instead of four.
 */
typedef uint8_t pch_dma_irq_index_t;

#endif
