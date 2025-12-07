/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "lwip/tcp.h"
#include "lwip/apps/mqtt.h"

void subscribe_sync(mqtt_client_t *client, const char *topic);
bool dns_lookup_sync(const char *host, ip_addr_t *host_addr);
bool mqtt_connect_sync(mqtt_client_t *client, const ip_addr_t *mqtt_server_addr, uint16_t mqtt_server_port, struct mqtt_connect_client_info_t *ci);
