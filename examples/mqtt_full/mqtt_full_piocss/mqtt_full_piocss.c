/*
 * Copyright (c) 2025-2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include <stdio.h>
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "pico/binary_info.h"
#include "pico/status_led.h"

#include "picochan/css.h"

#define NUM_MQTT_DEVS 8

// Use PIO0 via GPIO pins 0-3 in piochan order.
#define MQTT_PIO pio0
#define MQTT_TX_CLOCK_IN_PIN   0
#define MQTT_TX_DATA_OUT_PIN   1
#define MQTT_RX_CLOCK_OUT_PIN  2
#define MQTT_RX_DATA_IN_PIN    3

/*
 * mqtt_full_piocss runs the CSS side of the full MQTT Picochan
 * example and is configured to run on core 0 and connect to an
 * mqtt_full CU instance via a PIO channel connected to PIO0 via GPIO
 * pins 0-3. A physical connection is needed to a separate Pico that
 * is hosting a PIO CU via that connection with mqtt_full devices on
 * unit addresses 0, 1, 2 (at least), such as the mqtt_full_piocu
 * example program.
 */

const pch_chpid_t CHPID = 0;

#define MQTT_ENABLE_TRACE true

#ifdef MQTT_ENABLE_TRACE
#define MQTT_CHP_TRACE_FLAGS PCH_CHP_TRACED_MASK
//#define MQTT_CHP_TRACE_FLAGS PCH_CHP_TRACED_GENERAL
#else
#define MQTT_CHP_TRACE_FLAGS 0
#endif

static void light_led_ms(uint32_t ms) {
        status_led_init();
        status_led_set_state(true);
        sleep_ms(ms);
        status_led_set_state(false);
}

void io_cb(pch_intcode_t ic, pch_scsw_t scsw);
void run_css_example(void);

int main(void) {
        bi_decl(bi_program_description("picochan mqtt_full piocss CSS"));
        bi_decl(bi_4pins_with_names(
                MQTT_TX_CLOCK_IN_PIN, "TX_CLOCK_IN",
                MQTT_TX_DATA_OUT_PIN, "TX_DATA_OUT",
                MQTT_RX_CLOCK_OUT_PIN, "RX_CLOCK_OUT",
                MQTT_RX_DATA_IN_PIN, "RX_DATA_IN"));

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();
        light_led_ms(2000);
        printf("started main on core0\n");

        pch_css_init();
        pch_css_set_trace(MQTT_ENABLE_TRACE);
        pch_css_start(io_cb, 0); // start with callbacks disabled for all ISCs

        pch_pio_config_t cfg = pch_pio_get_default_config(MQTT_PIO);
        pch_piochan_init(&cfg);

        pch_piochan_pins_t pins = {
                .tx_clock_in = MQTT_TX_CLOCK_IN_PIN,
                .tx_data_out = MQTT_TX_DATA_OUT_PIN,
                .rx_clock_out = MQTT_RX_CLOCK_OUT_PIN,
                .rx_data_in = MQTT_RX_DATA_IN_PIN
        };
        pch_piochan_config_t pc = pch_piochan_get_default_config(pins);

        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 3); // allocates SIDs 0-2
        pch_chp_set_trace_flags(chpid, MQTT_CHP_TRACE_FLAGS);
        pch_chp_configure_piochan(chpid, &cfg, &pc);

        for (pch_sid_t sid = 0; sid < 3; sid++) {
                pch_sch_modify_enabled(sid, true);
                pch_sch_modify_traced(sid, MQTT_ENABLE_TRACE);
        }

        pch_chp_start(chpid);

        run_css_example();
        // NOTREACHED
}
