/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

/*! \file picochan/dev_status.h
 *  \ingroup picochan_cu
 *
 * \brief Device status bit values
 *
 * The device status is an 8-bit architected value that is sent from
 * a device (via its CU) to the CSS at the end of (and sometimes
 * during) the device's execution of a CCW. The device status sent
 * by the device is never modified by the CU or CSS but its bits
 * drive the CSS logic for how to progress/end the channel program
 * and the final device status of a channel program is visible to
 * the application in the SCSW part of the architected schib.
 */ 
#ifndef _PCH_DEV_STATUS_H
#define _PCH_DEV_STATUS_H

#define PCH_DEVS_ATTENTION        0x80
#define PCH_DEVS_STATUS_MODIFIER  0x40
#define PCH_DEVS_CONTROL_UNIT_END 0x20
#define PCH_DEVS_BUSY             0x10
#define PCH_DEVS_CHANNEL_END      0x08
#define PCH_DEVS_DEVICE_END       0x04
#define PCH_DEVS_UNIT_CHECK       0x02
#define PCH_DEVS_UNIT_EXCEPTION   0x01

#endif
