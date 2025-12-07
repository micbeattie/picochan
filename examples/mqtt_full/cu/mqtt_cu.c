/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <string.h>

#include "pico/stdlib.h"
#include "pico/status_led.h"
#include "pico/cyw43_arch.h"

#include "lwip/tcp.h"

#include "lwip/altcp_tcp.h"
#include "lwip/apps/mqtt.h"

#include "lwip/apps/mqtt_priv.h"

#include "picochan/hldev.h"
#include "picochan/ccw.h"

#include "mqtt_cu.h"
#include "mqtt_util.h"

#define MD_ENABLE_HLDEV_TRACE true

static pch_hldev_t *md_get_hldev(pch_hldev_config_t *hdcfg, int i) {
        mqtt_cu_config_t *cfg = (mqtt_cu_config_t *)hdcfg;
        return &cfg->mds[i].hldev;
}

mqtt_cu_config_t the_mqtt_cu_config = {
        .hldev_config = {
                .get_hldev = md_get_hldev,
                .start = md_hldev_callback
        }
};

md_cu_stats_t md_cu_statistics;

void md_wake(mqtt_cu_config_t *cfg, mqtt_dev_t *md) {
        pch_devib_t *devib = md_get_devib(cfg, md);
        if (pch_devib_is_started(devib)) {
                uint8_t ccwcmd = md->hldev.ccwcmd;
                assert(ccwcmd);
                if (ccwcmd == CMD(WAIT))
                        pch_hldev_end_ok(devib);
        } else {
                // Send an unsolicited ATTENTION device status
                uint8_t devstat = PCH_DEVS_ATTENTION | PCH_DEVS_DEVICE_END;
                pch_dev_update_status(devib, devstat);
        }
}

// CU initialisation

pch_unit_addr_t mqtt_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices) {
        if (num_devices > NUM_MQTT_DEVS)
                num_devices = NUM_MQTT_DEVS;

        pch_hldev_config_init(&the_mqtt_cu_config.hldev_config,
                cu, first_ua, num_devices);

        pch_dev_range_set_traced(&the_mqtt_cu_config.hldev_config.dev_range,
                MD_ENABLE_HLDEV_TRACE);

        return first_ua + num_devices;
}
