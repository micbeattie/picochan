/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "pico/binary_info.h"
#include "pico/status_led.h"
#include "pico/cyw43_arch.h"

#include "../mqtt_cu.h"

/*
 * mqtt_full_piocu runs the CU side of the mqtt_full Picochan example
 * and is configured to run on core 0 and serve up its MQTT devices via
 * a PIO channel connected to GPIO pins 0-3 in "piochan order", i.e.
 * respectively TX_CLOCK_IN, TX_DATA_OUT, RX_CLOCK_OUT, RX_DATA_IN.
 * A physical connection is needed to a separate Pico that is running
 * a CSS configured to use a PIO channel for that connection with the
 * appropriate pin connections, i.e. TX_CLOCK_IN<->RX_CLOCK_OUT and
 * TX_DATA_OUT<->RX_DATA_IN, such as the mqtt_full_piocss example program.
 */

const pch_unit_addr_t FIRST_UA = 0;
const pch_cuaddr_t CUADDR = 0;

#define MQTT_ENABLE_TRACE true

#ifdef MQTT_ENABLE_TRACE
#define MQTT_CU_TRACE_FLAGS PCH_CU_TRACED_MASK
//#define MQTT_CU_TRACE_FLAGS PCH_CU_TRACED_GENERAL
#else
#define MQTT_CU_TRACE_FLAGS 0
#endif

// Use PIO0 via GPIO pins 0-3 in piochan order.
#define MQTT_PIO pio0
#define MQTT_TX_CLOCK_IN_PIN   0
#define MQTT_TX_DATA_OUT_PIN   1
#define MQTT_RX_CLOCK_OUT_PIN  2
#define MQTT_RX_DATA_IN_PIN    3

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

int main(void) {
        bi_decl(bi_program_description("picochan mqtt_full CU"));
        bi_decl(bi_4pins_with_names(
                MQTT_TX_CLOCK_IN_PIN, "TX_CLOCK_IN",
                MQTT_TX_DATA_OUT_PIN, "TX_DATA_OUT",
                MQTT_RX_CLOCK_OUT_PIN, "RX_CLOCK_OUT",
                MQTT_RX_DATA_IN_PIN, "RX_DATA_IN"));

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();

        int err = cyw43_arch_init();
        if (err != 0)
                panic("cyw43_arch_init");

        light_led_ms(1000);

        wifi_connect();
        pch_cus_init();
        pch_cus_set_trace(MQTT_ENABLE_TRACE);
        
        mqtt_cu_init(&mqtt_cu, FIRST_UA, NUM_MQTT_DEVS);
        pch_cu_register(&mqtt_cu, CUADDR);
        pch_cu_set_trace_flags(CUADDR, MQTT_CU_TRACE_FLAGS);

        pch_pio_config_t cfg = pch_pio_get_default_config(MQTT_PIO);
        pch_piochan_init(&cfg);

        pch_piochan_pins_t pins = {
                .tx_clock_in = MQTT_TX_CLOCK_IN_PIN,
                .tx_data_out = MQTT_TX_DATA_OUT_PIN,
                .rx_clock_out = MQTT_RX_CLOCK_OUT_PIN,
                .rx_data_in = MQTT_RX_DATA_IN_PIN
        };
        pch_piochan_config_t pc = pch_piochan_get_default_config(pins);

        pch_cus_piocu_configure(CUADDR, &cfg, &pc);
        pch_cu_start(CUADDR);
        printf("CU ready\n");

        while (1) {
                mqtt_cu_poll();
                sleep_ms(5); // XXX help or hinder?
        }
}
