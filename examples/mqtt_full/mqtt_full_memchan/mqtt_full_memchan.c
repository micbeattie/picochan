/*
 * Copyright (c) 2025-2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include <stdio.h>
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/status_led.h"
#include "pico/cyw43_arch.h"

#include "picochan/css.h"
#include "picochan/cu.h"

#define NUM_MQTT_DEVS 8

#include "../mqtt_cu.h"
#include "../mqtt_api.h"

/*
 * mqtt_full_memchan runs the complete mqtt Picochan example on a
 * single Pico. The CSS is run on core 0 and the CU on core 1.
 * Instead of needing physical channel connections between CSS
 * and CU, this configuration uses a memory channel (memchan)
 * so that CSS-to-CU communication happens directly via
 * memory-to-memory DMA for data transfers and 4-byte
 * writes/reads from memory for command transfers.
 */

const pch_unit_addr_t FIRST_UA = 0;
const pch_cuaddr_t CUADDR = 0;
const pch_chpid_t CHPID = 0;

#define MQTT_ENABLE_TRACE true

#ifdef MQTT_ENABLE_TRACE
//#define MQTT_CU_TRACE_FLAGS PCH_CU_TRACED_MASK
//#define MQTT_CHP_TRACE_FLAGS PCH_CHP_TRACED_MASK
#define MQTT_CU_TRACE_FLAGS PCH_CU_TRACED_GENERAL
#define MQTT_CHP_TRACE_FLAGS PCH_CHP_TRACED_GENERAL
#else
#define MQTT_CU_TRACE_FLAGS 0
#define MQTT_CHP_TRACE_FLAGS 0
#endif

static pch_cu_t mqtt_cu = PCH_CU_INIT(NUM_MQTT_DEVS);

static void light_led_ms(uint32_t ms) {
        status_led_init_with_context(cyw43_arch_async_context());
        status_led_set_state(true);
        sleep_ms(ms);
        status_led_set_state(false);
}

static void wifi_connect(void) {
        cyw43_arch_enable_sta_mode();

        printf("connecting to WiFi...\n");
        int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID,
                WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
        if (err != 0) {
                printf("connect to WiFi failed: err=%d\n", err);
                panic("WiFi connect failed");
        }

        printf("connected to WiFi\n");
}

bool core1_ready;

static void core1_thread(void) {
        int err = cyw43_arch_init();
        if (err != 0)
                panic("cyw43_arch_init");

        light_led_ms(1000);

        wifi_connect();
        pch_cus_init(); // could do from core 0
        pch_cus_set_trace(MQTT_ENABLE_TRACE); // could do from core 0
        
        mqtt_cu_init(&mqtt_cu, FIRST_UA, NUM_MQTT_DEVS);
        pch_cu_register(&mqtt_cu, CUADDR);
        pch_cu_set_trace_flags(CUADDR, MQTT_CU_TRACE_FLAGS);

        pch_channel_t *chpeer = pch_chp_get_channel(CHPID);
        pch_cus_memcu_configure(CUADDR, chpeer);

        pch_cu_start(CUADDR);

        printf("CU ready\n");
        core1_ready = true; // core0 waits for this

        while (1) {
                mqtt_cu_poll();
                sleep_ms(5); // XXX help or hinder?
        }
}

void io_cb(pch_intcode_t ic, pch_scsw_t scsw);
void run_css_example(void);

int main(void) {
        bi_decl(bi_program_description("picochan mqtt_full memchan CSS+CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();
        sleep_ms(2000);
        printf("started main on core0\n");

        pch_memchan_init();

        pch_css_init();
        pch_css_set_trace(MQTT_ENABLE_TRACE);
        pch_css_start(io_cb, 0); // start with callbacks disabled for all ISCs
        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 3); // allocates SIDs 0-2
        pch_chp_set_trace_flags(chpid, MQTT_CHP_TRACE_FLAGS);

        printf("starting core1 and waiting for it to be ready...\n");
        multicore_launch_core1(core1_thread);
        while (!core1_ready)
                sleep_ms(1);

        printf("core0 continuing\n");

        pch_channel_t *chpeer = pch_cu_get_channel(CUADDR);
        pch_chp_configure_memchan(CHPID, chpeer);

        for (pch_sid_t sid = 0; sid < 3; sid++) {
                pch_sch_modify_enabled(sid, true);
                pch_sch_modify_traced(sid, MQTT_ENABLE_TRACE);
        }

        pch_chp_start(chpid);

        run_css_example();
        // NOTREACHED
}
