/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/sync.h"

#include "lwip/tcp.h"
#include "lwip/dns.h"

#include "lwip/altcp_tcp.h"
#include "lwip/apps/mqtt.h"

static void sub_request_cb(void *arg, err_t err) {
        *(err_t *)arg = err;
}

void subscribe_sync(mqtt_client_t *client, const char *topic) {
        printf("subscribing to %s\n", topic);
        err_t err = ERR_INPROGRESS;
        mqtt_sub_unsub(client, topic, 0, sub_request_cb, &err, 1);
        while (err == ERR_INPROGRESS) {
                cyw43_arch_poll();
                sleep_ms(1);
        }

        if (err == ERR_OK) {
                printf("subscribed ok\n");
        } else {
                printf("subscribe failed with err=%d\n", err);
                panic("subscribe");
        }
}

static void dns_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
        *(ip_addr_t *)arg = *ipaddr;
}

bool dns_lookup_sync(const char *host, ip_addr_t *host_addr) {
        printf("Running DNS query for %s\n", host);
        host_addr->addr = 0;

        cyw43_arch_lwip_begin();
        err_t err = dns_gethostbyname(host, host_addr, dns_cb, host_addr);
        cyw43_arch_lwip_end();

        if (err == ERR_ARG) {
                printf("failed to start DNS query\n");
                return false;
        }

        if (err == ERR_OK) {
                printf("no need to wait for DNS\n");
        } else {
                if (err == ERR_INPROGRESS)
                        printf("DNS lookup is in progress\n");
                else
                        printf("err=%d but will wait anyway\n", err);

                printf("waiting for DNS...\n");
                while (host_addr->addr == 0) {
                        cyw43_arch_poll();
                        sleep_ms(1);
                }
        }

        printf("IP address is %s\n", ip4addr_ntoa(host_addr));
        return true;
}

static void connect_cb(mqtt_client_t *c, void *arg, mqtt_connection_status_t status) {
        *(int *)arg = (int)status;
}

bool mqtt_connect_sync(mqtt_client_t *client, const ip_addr_t *addr, uint16_t port, struct mqtt_connect_client_info_t *ci) {
        printf("Connecting to MQTT server...\n");
        int status = -1;
        err_t err = mqtt_client_connect(client, addr, port,
                connect_cb, &status, ci);
        printf("mqtt_client_connect returned err %d\n", err);

        while (status == -1) {
                cyw43_arch_poll();
                sleep_ms(1);
        }

        printf("connection status ready: status is now %d\n", status);
        return !status;
}
