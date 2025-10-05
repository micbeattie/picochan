/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
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

/*! \brief a control unit address between 0 and PCH_NUM_CUS-1
 * (at most 255) that identifies a control unit from the CU side.
 *  \ingroup picochan_base
 */
typedef uint8_t pch_cuaddr_t;

/*! \brief a unit address that identifies a device on a given CU on
 * the control unit side.
 *  \ingroup picochan_base
 *
 *  Must be between 0 and cu->num_devibs-1 (which is at most 255).
 */
typedef uint8_t pch_unit_addr_t;

/*! \brief a channel path identifier between 0 and PCH_NUM_CHANNELS-1
 * (at most 255) that identifies a channel from the CSS side
 *  \ingroup picochan_base
 *
 * Each channel connects to a single remote CU.
 */
typedef uint8_t pch_chpid_t;

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
 *  Must be either -1 (meaning no DMA IRQ index set) or between 0 and
 *  the number of DMA IRQs on the platform (e.g. 2 for RP2040 and 4
 *  for RP2350). Pico SDK uses the uint type for DMA IRQ index
 *  arguments but Picochan uses the pch_dma_irq_index_t type in its
 *  API and also for storing them so it can use a single byte instead
 *  of four.
 */
typedef int8_t pch_dma_irq_index_t;

#endif
