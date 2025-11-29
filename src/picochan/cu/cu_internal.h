/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CUS_CU_INTERNAL_H
#define _PCH_CUS_CU_INTERNAL_H

#include "picochan/cu.h"
#include "proto/packet.h"
#include "devibs_lock.h"

static inline void pch_cu_set_flag_configured(pch_cu_t *cu, bool b) {
        if (b)
                cu->flags |= PCH_CU_CONFIGURED;
        else
                cu->flags &= ~PCH_CU_CONFIGURED;
}

static inline void pch_cu_set_flag_started(pch_cu_t *cu, bool b) {
        if (b)
                cu->flags |= PCH_CU_STARTED;
        else
                cu->flags &= ~PCH_CU_STARTED;
}

static inline void pch_dev_update_status_proto_error(pch_devib_t *devib) {
        pch_dev_update_status_error(devib, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_PROTO_ERROR,
                .code = devib->op,
                .asc = devib->payload.p0,
                .ascq = devib->payload.p1
        }));
}

void pch_cus_send_command_to_css(pch_cu_t *cu);
void pch_cus_handle_rx_complete(pch_cu_t *cu);
void pch_cus_handle_tx_complete(pch_cu_t *cu);

#endif
