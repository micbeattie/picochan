/*
 * Copyright (c) 2025-2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#ifndef _MQTT_CU_API_H
#define _MQTT_CU_API_H

#include "picochan/cu.h"

#ifndef NUM_MQTT_DEVS
#define NUM_MQTT_DEVS 8
#endif

pch_unit_addr_t mqtt_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices);

void mqtt_cu_poll(void);
#endif
