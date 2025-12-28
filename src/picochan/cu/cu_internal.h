/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CUS_CU_INTERNAL_H
#define _PCH_CUS_CU_INTERNAL_H

#include "pico/async_context_threadsafe_background.h"
#include "picochan/cu.h"
#include "proto/packet.h"
#include "devibs_lock.h"

extern async_context_t *pch_cus_async_context;

static inline void pch_dev_update_status_proto_error(pch_devib_t *devib) {
        pch_dev_update_status_error(devib, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_PROTO_ERROR,
                .code = devib->op,
                .asc = devib->payload.p0,
                .ascq = devib->payload.p1
        }));
}

static inline void pch_cu_schedule_worker(pch_cu_t *cu) {
        async_context_set_work_pending(cu->async_context, &cu->worker);
}

static inline void pch_devib_schedule_callback(pch_devib_t *devib) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        pch_cu_push_devib(cu, &cu->cb_list, devib);
        pch_cu_schedule_worker(cu);
}

void pch_cus_async_worker_callback(async_context_t *context, async_when_pending_worker_t *worker);

void pch_cu_send_pending_tx_command(pch_cu_t *cu, pch_devib_t *devib);
void pch_cus_handle_rx_complete(pch_cu_t *cu);
void pch_cus_handle_tx_complete(pch_cu_t *cu);

#endif
