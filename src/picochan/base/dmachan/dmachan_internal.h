/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_DMACHAN_DMACHAN_INTERNAL_H
#define _PCH_DMACHAN_DMACHAN_INTERNAL_H

#include "hardware/sync.h"
#include "picochan/dmachan.h"
#include "dmachan_trace.h"

void dmachan_handle_rx_resetting(dmachan_rx_channel_t *rx);

void dmachan_init_tx_channel(dmachan_tx_channel_t *tx, dmachan_1way_config_t *d1c, const dmachan_tx_channel_ops_t *ops);
void dmachan_init_rx_channel(dmachan_rx_channel_t *rx, dmachan_1way_config_t *d1c, const dmachan_rx_channel_ops_t *ops);

#endif
