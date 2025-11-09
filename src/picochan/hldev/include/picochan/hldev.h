/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_HLDEV_HLDEV_H
#define _PCH_HLDEV_HLDEV_H

#include "picochan/cu.h"

/*! \file picochan/hldev.h
 *  \ingroup picochan_hldev
 *
 * \brief A higher level API for implementing devices on a CU
 */

// values for pch_hldev_t state field
#define PCH_HLDEV_IDLE          0
#define PCH_HLDEV_STARTED       1
#define PCH_HLDEV_RECEIVING     2
#define PCH_HLDEV_SENDING       3
#define PCH_HLDEV_SENDING_FINAL 4

// values for code fields of dev_sense_t for PCH_DEV_SENSE_PROTO_ERROR
#define PCH_HLDEV_ERR_NO_START_CALLBACK         1
#define PCH_HLDEV_ERR_RECEIVE_FROM_READ_CCW     2
#define PCH_HLDEV_ERR_SEND_TO_WRITE_CCW         3
#define PCH_HLDEV_ERR_IDLE_OP_NOT_START         4

typedef struct pch_hldev_config pch_hldev_config_t;
typedef struct pch_hldev pch_hldev_t;

typedef pch_hldev_t *(*pch_hldev_getter_t)(pch_hldev_config_t *hdcfg, int i);
typedef void (*pch_hldev_callback_t)(pch_hldev_config_t *hdcfg, pch_devib_t *devib);

typedef struct pch_hldev_config {
        pch_dev_range_t         dev_range;
        pch_hldev_getter_t      get_hldev;
        pch_hldev_callback_t    start;
        pch_hldev_callback_t    signal;
} pch_hldev_config_t;

typedef struct pch_hldev {
        pch_hldev_callback_t    callback;
        void                    *addr; // dest/source address for receive/send
        uint16_t                size;  // total bytes to receive/send
        uint16_t                count; // bytes received/sent so far
        uint8_t                 state;
        uint8_t                 flags;
        uint8_t                 ccwcmd;
} pch_hldev_t;

// values for pch_hldev_t flags
// PCH_HLDEV_FLAG_EOF indicates that no more data is available to be
// received from a Write-type CCW
#define PCH_HLDEV_FLAG_EOF      0x01

static inline bool pch_hldev_is_idle(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_IDLE;
}

static inline bool pch_hldev_is_started(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_STARTED;
}

static inline bool pch_hldev_is_receiving(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_RECEIVING;
}

static inline bool pch_hldev_is_sending(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_SENDING;
}

static inline bool pch_hldev_is_sending_final(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_SENDING_FINAL;
}

static inline int pch_hldev_get_index(pch_hldev_config_t *hdcfg, pch_devib_t *devib) {
        return pch_dev_range_get_index(&hdcfg->dev_range, devib);
}

static inline int pch_hldev_get_index_required(pch_hldev_config_t *hdcfg, pch_devib_t *devib) {
        return pch_dev_range_get_index_required(&hdcfg->dev_range, devib);
}

static inline pch_hldev_t *pch_hldev_get(pch_hldev_config_t *hdcfg, pch_devib_t *devib) {
        int i = pch_hldev_get_index(hdcfg, devib);
        if (i == -1)
                return NULL;

        return hdcfg->get_hldev(hdcfg, i);
}

static inline pch_hldev_t *pch_hldev_get_required(pch_hldev_config_t *hdcfg, pch_devib_t *devib) {
        int i = pch_hldev_get_index_required(hdcfg, devib);
        return hdcfg->get_hldev(hdcfg, i);
}

static inline pch_devib_t *pch_hldev_get_devib(pch_hldev_config_t *hdcfg, int i) {
        return pch_dev_range_get_devib_by_index_required(&hdcfg->dev_range, i);
}

void pch_hldev_receive_then(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *dstaddr, uint16_t size, pch_hldev_callback_t callback);
void pch_hldev_receive(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *dstaddr, uint16_t size);
void pch_hldev_call_callback(pch_hldev_config_t *hdcfg, pch_devib_t *devib);
void pch_hldev_send_then(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *srcaddr, uint16_t size, pch_hldev_callback_t callback);
void pch_hldev_send_final(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *srcaddr, uint16_t size);
void pch_hldev_send(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *srcaddr, uint16_t size);
void pch_hldev_end(pch_hldev_config_t *hdcfg, pch_devib_t *devib, uint8_t extra_devs, pch_dev_sense_t sense);
void pch_hldev_end_ok(pch_hldev_config_t *hdcfg, pch_devib_t *devib);
void pch_hldev_terminate_string(pch_hldev_config_t *hdcfg, pch_devib_t *devib);
void pch_hldev_terminate_string_end_ok(pch_hldev_config_t *hdcfg, pch_devib_t *devib);
void pch_hldev_receive_buffer_final(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *dstaddr, uint16_t size);
void pch_hldev_receive_string_final(pch_hldev_config_t *hdcfg, pch_devib_t *devib, void *dstaddr, uint16_t len);


static inline void pch_hldev_end_ok_sense(pch_hldev_config_t *hdcfg, pch_devib_t *devib, pch_dev_sense_t sense) {
        pch_hldev_end(hdcfg, devib, 0, sense);
}

static inline void pch_hldev_end_reject(pch_hldev_config_t *hdcfg, pch_devib_t *devib, uint8_t code) {
        pch_hldev_end(hdcfg, devib, 0, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_COMMAND_REJECT,
                .code = code
        }));
}

static inline void pch_hldev_end_exception_sense(pch_hldev_config_t *hdcfg, pch_devib_t *devib, pch_dev_sense_t sense) {
        pch_hldev_end(hdcfg, devib, PCH_DEVS_UNIT_EXCEPTION, sense);
}

static inline void pch_hldev_end_exception(pch_hldev_config_t *hdcfg, pch_devib_t *devib) {
        pch_hldev_end_exception_sense(hdcfg, devib, PCH_DEV_SENSE_NONE);
}

static inline void pch_hldev_end_intervention(pch_hldev_config_t *hdcfg, pch_devib_t *devib, uint8_t code) {
        pch_hldev_end(hdcfg, devib, 0, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_INTERVENTION_REQUIRED,
                .code = code
        }));
}

static inline void pch_hldev_end_equipment_check(pch_hldev_config_t *hdcfg, pch_devib_t *devib, uint8_t code) {
        pch_hldev_end(hdcfg, devib, 0, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_EQUIPMENT_CHECK,
                .code = code
        }));
}

static inline void pch_hldev_end_stopped(pch_hldev_config_t *hdcfg, pch_devib_t *devib) {
        pch_hldev_end(hdcfg, devib, 0, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_CANCEL
        }));
}

void pch_hldev_dev_range_init(pch_dev_range_t *dr, pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices, pch_devib_callback_t start_devib);
void pch_hldev_config_init(pch_hldev_config_t *hdcfg, pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices, pch_devib_callback_t start_devib);

#endif
