/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#ifndef _MD_API_H
#define _MD_API_H

#include "picochan/cu.h"

#define MD_CCW_CMD_SET_TOPIC 0x03

void mqtt_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices);

void mqtt_cu_poll(void);

bool mqtt_connect_cu_sync(const char *mqtt_server_host, uint16_t mqtt_server_port, const char *mqtt_username, const char *mqtt_password);

#endif
