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

/*! \brief a control unit number between 0 and PCH_NUM_CSS_CUS-1
 * (at most 255) that identifies a control unit from the CSS side
 *  \ingroup picochan_base
 *
 * The CSS may have multiple channels each to an entirely independent
 * remote CU. In this situation, each CU-side CU may refer to itself
 * with a control unit address (pch_cuaddr_t) of 0 whereas each
 * corresponding CSS-side CU (css_cu_t) will have a unique control
 * unit number (pch_cunum_t).
 */
typedef uint8_t pch_cunum_t;

/*! \brief a device number that identifies a device by its (CSS-side)
 * control unit number (pch_cunum_t_ in the most significant byte and
 * its unit address (pch_unit_addr_t) on the corresponding CU-side CU
 * in the least significant byte.
 *  \ingroup picochan_base
 */
typedef uint16_t pch_devno_t;

static inline pch_cunum_t pch_devno_get_cunum(pch_devno_t devno) {
        return devno >> 8;
}

static inline pch_unit_addr_t pch_devno_get_ua(pch_devno_t devno) {
        return devno & 0xff;
}

static inline pch_devno_t pch_make_devno(pch_cunum_t cunum, pch_unit_addr_t ua) {
        return ((pch_devno_t)cunum << 8) | ua;
}

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
