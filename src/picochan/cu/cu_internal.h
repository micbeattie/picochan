/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CUS_CU_INTERNAL_H
#define _PCH_CUS_CU_INTERNAL_H

#include "picochan/cu.h"
#include "proto/packet.h"
#include "devibs_lock.h"

static inline proto_packet_t get_rx_packet(pch_cu_t *cu) {
        // cu.rx_channel is a dmachan_rx_channel_t which is
        // __aligned(4) and cmd is the first member of rx_channel
        // so is 4-byte aligned. proto_packet_t is 4-bytes and also
        // __aligned(4) (and needing no more than 4-byte alignment)
        // but omitting the __builtin_assume_aligned below causes
        // gcc 14.1.0 to produce error
        // error: cast increases required alignment of target type
        // [-Werror=cast-align]
        proto_packet_t *pp = (proto_packet_t *)
                __builtin_assume_aligned(&cu->rx_channel.link.cmd, 4);
        return *pp;
}

void pch_cus_send_command_to_css(pch_cu_t *cu);
void pch_cus_handle_rx_complete(pch_cu_t *cu);
void pch_cus_handle_tx_complete(pch_cu_t *cu);

#endif
