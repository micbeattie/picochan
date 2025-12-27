/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_HLDEV_HLDEV_TRACE_H
#define _PCH_HLDEV_HLDEV_TRACE_H

#include "picochan/devib.h"
#include "picochan/cu.h"
#include "picochan/trc_records.h"
#include "picochan/hldev.h"

static inline bool hcdcfg_or_hldev_is_traced(pch_devib_t *devib) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        return pch_dev_range_is_traced(&hdcfg->dev_range)
                || pch_hldev_is_traced(hd);
}

// Not using underlying trace macros for now - need to separate out
// trc into its own module to do that properly
#define PCH_HLDEV_TRACE_COND(rt, cond, data) \
        do { \
                if (cond) \
                        pch_cus_trace_write_user(rt, &(data), sizeof(data)); \
        } while (0)

static inline void trace_hldev_config_init(pch_hldev_config_t *hdcfg) {
        pch_dev_range_t *dr = &hdcfg->dev_range;
        pch_cu_t *cu = dr->cu;
        pch_devib_t *first_devib = pch_get_devib(cu, dr->first_ua);
        PCH_HLDEV_TRACE_COND(PCH_TRC_RT_HLDEV_CONFIG_INIT,
                pch_cus_is_traced(),
                ((struct pch_trdata_hldev_config_init){
                        .hdcfg = (uint32_t)hdcfg,
                        .start = (uint32_t)hdcfg->start,
                        .signal = (uint32_t)hdcfg->signal,
                        .cuaddr = cu->cuaddr,
                        .first_ua = dr->first_ua,
                        .num_devices = dr->num_devices,
                        .cbindex = first_devib->cbindex
                }));
}

static inline void trace_hldev_start(pch_devib_t *devib) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        pch_cuaddr_t cuaddr = pch_dev_get_cuaddr(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        PCH_HLDEV_TRACE_COND(PCH_TRC_RT_HLDEV_START,
                pch_dev_range_is_traced(&hdcfg->dev_range)
                || pch_hldev_is_traced(hd),
                ((struct pch_trdata_hldev_start){
                        .cuaddr = cuaddr,
                        .ua = ua,
                        .ccwcmd = devib->payload.p0,
                        .esize = devib->payload.p1
                }));
}

static inline void trace_hldev_count(pch_trc_record_type_t rt, pch_devib_t *devib, uint16_t count) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        pch_cuaddr_t cuaddr = pch_dev_get_cuaddr(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        PCH_HLDEV_TRACE_COND(rt,
                pch_dev_range_is_traced(&hdcfg->dev_range)
                || pch_hldev_is_traced(hd),
                ((struct pch_trdata_count_dev){
                        .cuaddr = cuaddr,
                        .ua = ua,
                        .count = count, 
                }));
}

static inline void trace_hldev_counts(pch_trc_record_type_t rt, pch_devib_t *devib, uint16_t count1, uint16_t count2) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        pch_cuaddr_t cuaddr = pch_dev_get_cuaddr(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        PCH_HLDEV_TRACE_COND(rt,
                pch_dev_range_is_traced(&hdcfg->dev_range)
                || pch_hldev_is_traced(hd),
                ((struct pch_trdata_counts_dev){
                        .cuaddr = cuaddr,
                        .ua = ua,
                        .count1 = count1,
                        .count2 = count2
                }));
}

static inline void trace_hldev_byte(pch_trc_record_type_t rt, pch_devib_t *devib, uint8_t byte) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        pch_cuaddr_t cuaddr = pch_dev_get_cuaddr(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        PCH_HLDEV_TRACE_COND(rt,
                pch_dev_range_is_traced(&hdcfg->dev_range)
                || pch_hldev_is_traced(hd),
                ((struct pch_trdata_dev_byte){
                        .cuaddr = cuaddr,
                        .ua = ua,
                        .byte = byte
                }));
}

static inline void trace_hldev_data(pch_trc_record_type_t rt, pch_devib_t *devib, void *addr, uint16_t count) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        pch_cuaddr_t cuaddr = pch_dev_get_cuaddr(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        PCH_HLDEV_TRACE_COND(rt,
                pch_dev_range_is_traced(&hdcfg->dev_range)
                || pch_hldev_is_traced(hd),
                ((struct pch_trdata_hldev_data){
                        .cuaddr = cuaddr,
                        .ua = ua,
                        .count = count, 
                        .addr = (uint32_t)addr
                }));
}

static inline void trace_hldev_data_then(pch_trc_record_type_t rt, pch_devib_t *devib, void *addr, uint16_t count, pch_devib_callback_t cbaddr) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        pch_cuaddr_t cuaddr = pch_dev_get_cuaddr(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        PCH_HLDEV_TRACE_COND(rt,
                pch_dev_range_is_traced(&hdcfg->dev_range)
                || pch_hldev_is_traced(hd),
                ((struct pch_trdata_hldev_data_then){
                        .cuaddr = cuaddr,
                        .ua = ua,
                        .count = count, 
                        .addr = (uint32_t)addr,
                        .cbaddr = (uint32_t)cbaddr
                }));
}

static inline void trace_hldev_end(pch_devib_t *devib, pch_dev_sense_t sense, uint8_t devstat) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        pch_hldev_t *hd = pch_hldev_get(devib);
        pch_cuaddr_t cuaddr = pch_dev_get_cuaddr(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        PCH_HLDEV_TRACE_COND(PCH_TRC_RT_HLDEV_END,
                pch_dev_range_is_traced(&hdcfg->dev_range)
                || pch_hldev_is_traced(hd),
                ((struct pch_trdata_hldev_end){
                        .cuaddr = cuaddr,
                        .ua = ua,
                        .devstat = devstat,
                        // esize not set via pch_hldev_end() yet
                        .sense_flags = sense.flags,
                        .sense_code = sense.code,
                        .sense_asc = sense.asc,
                        .sense_ascq = sense.ascq
                }));
}
#endif

