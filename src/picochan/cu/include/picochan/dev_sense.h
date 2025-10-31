/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CU_DEV_SENSE_H
#define _PCH_CU_DEV_SENSE_H

/*! \file picochan/dev_sense.h
 *  \ingroup picochan_base
 *
 * \brief Device sense
 */

/*! \brief The device sense structure by which a device can communicate additional error information on request by the CSS
 *  \ingroup picochan_base
 */
typedef struct __attribute__((__packed__,__aligned__(4))) pch_dev_sense {
        uint8_t flags;
        uint8_t code;
        uint8_t asc;
        uint8_t ascq;
} pch_dev_sense_t;

#define PCH_DEV_SENSE_NONE ((pch_dev_sense_t){0})

#define PCH_DEV_SENSE_COMMAND_REJECT            0x80
#define PCH_DEV_SENSE_INTERVENTION_REQUIRED     0x40
#define PCH_DEV_SENSE_BUS_OUT_CHECK             0x20
#define PCH_DEV_SENSE_EQUIPMENT_CHECK           0x10
#define PCH_DEV_SENSE_DATA_CHECK                0x08
#define PCH_DEV_SENSE_OVERRUN                   0x04
#define PCH_DEV_SENSE_PROTO_ERROR               0x02
#define PCH_DEV_SENSE_CANCEL                    0x01

#endif
