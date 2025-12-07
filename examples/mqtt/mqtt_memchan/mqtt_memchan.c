/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/irq.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/status_led.h"
#include "pico/cyw43_arch.h"

#include "picochan/css.h"
#include "picochan/cu.h"

#include "../md_api.h"

/*
 * mqtt_memchan runs the complete mqtt Picochan example on a
 * single Pico. The CSS is run on core 0 and the CU on core 1.
 * Instead of needing physical channel connections between CSS
 * and CU, this configuration uses a memory channel (memchan)
 * so that CSS-to-CU communication happens directly via
 * memory-to-memory DMA for data transfers and 4-byte
 * writes/reads from memory for command transfers.
 */

#define NUM_MQTT_DEVS 8
const pch_unit_addr_t FIRST_UA = 0;
const pch_cuaddr_t CUADDR = 0;
const pch_chpid_t CHPID = 0;

#define MQTT_ENABLE_TRACE true

static pch_cu_t mqtt_cu = PCH_CU_INIT(NUM_MQTT_DEVS);

pch_dmaid_t css_to_cu_dmaid;
pch_dmaid_t cu_to_css_dmaid;
pch_dma_irq_index_t css_dmairqix = -1;
pch_dma_irq_index_t cu_dmairqix = -1;

static void light_led_for_three_seconds() {
        status_led_init_with_context(cyw43_arch_async_context());
        status_led_set_state(true);
        sleep_ms(3000);
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

        light_led_for_three_seconds();

        wifi_connect();
        pch_cus_init(); // could do from core 0
        pch_cus_set_trace(MQTT_ENABLE_TRACE); // could do from core 0
        pch_cus_configure_dma_irq_index_shared_default(cu_dmairqix);
        
        mqtt_cu_init(&mqtt_cu, FIRST_UA, NUM_MQTT_DEVS);
        pch_cu_register(&mqtt_cu, CUADDR);
        pch_cus_trace_cu(CUADDR, MQTT_ENABLE_TRACE);

        dmachan_tx_channel_t *txpeer = pch_chp_get_tx_channel(CHPID);
        pch_cus_memcu_configure(CUADDR, cu_to_css_dmaid,
                css_to_cu_dmaid, txpeer);

        pch_cu_start(CUADDR);

        if (!mqtt_connect_cu_sync(MQTT_SERVER_HOST, MQTT_SERVER_PORT,
                MQTT_USERNAME, MQTT_PASSWORD)) {
                panic("MQTT connect failed");
        }

        printf("CU ready\n");
        core1_ready = true; // core0 waits for this

        // just busy poll for mqtt work (which itself calls
        // cyw43_arch_poll() to poll for lwIP work)
        while (1)
                mqtt_cu_poll();
}

char topic[] = "pico/output";
char message[] = "Hello world";

static pch_ccw_t mqtt_chanprog[] = {
        { MD_CCW_CMD_SET_TOPIC, PCH_CCW_FLAG_CC|PCH_CCW_FLAG_SLI,
                sizeof(topic), (uint32_t)topic },
        { PCH_CCW_CMD_WRITE, 0,
                sizeof(message), (uint32_t)message }
};

int main(void) {
        bi_decl(bi_program_description("picochan mqtt memchan CSS+CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();
        sleep_ms(3000);
        printf("started main on core0\n");
        css_to_cu_dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        cu_to_css_dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        css_dmairqix = 0;
        cu_dmairqix = 1;

        pch_memchan_init();

        pch_css_init();
        pch_css_set_trace(MQTT_ENABLE_TRACE);
        pch_css_configure_dma_irq_index_shared(css_dmairqix,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        pch_css_start(NULL, 0); // must set CSS dmairqix before this
        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 1); // allocates SID 0
        pch_chp_set_trace(chpid, MQTT_ENABLE_TRACE);

        printf("starting core1 and waiting for it to be ready...\n");
        multicore_launch_core1(core1_thread);
        while (!core1_ready)
                sleep_ms(1);

        printf("core0 continuing\n");

        dmachan_tx_channel_t *txpeer = pch_cu_get_tx_channel(CUADDR);
        pch_chp_configure_memchan(CHPID, css_to_cu_dmaid,
                cu_to_css_dmaid, txpeer);

        pch_sch_modify_enabled(0, true);
        pch_sch_modify_traced(0, MQTT_ENABLE_TRACE);

        pch_chp_start(chpid);

        printf("starting channel program to publish to MQTT\n");
        pch_sch_start(0, mqtt_chanprog);

        while (1)
                __wfe();
}
