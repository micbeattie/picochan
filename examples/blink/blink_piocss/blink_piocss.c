/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/status_led.h"
#include "pico/binary_info.h"

#include "picochan/css.h"

/*
 * blink_piocss runs the CSS side of the blink Picochan example and is
 * configured to run on core 0 and connect to a blink CU instance via
 * a PIO channel connected to PIO0 via GPIO pins 0-3.
 * A physical connection is needed to a separate Pico that is hosting
 * a PIO CU via that connection with a blink device on unit address 0,
 * such as the blink_piocu example program.
 */

#define BLINK_ENABLE_TRACE true

// Use PIO0 via GPIO pins 0-3 in piochan order.
#define BLINK_PIO pio0
#define BLINK_TX_CLOCK_IN_PIN   0
#define BLINK_TX_DATA_OUT_PIN   1
#define BLINK_RX_CLOCK_OUT_PIN  2
#define BLINK_RX_DATA_IN_PIN    3

static void light_led_for_three_seconds() {
        status_led_init();
        status_led_set_state(true);
        sleep_ms(3000);
        status_led_set_state(false);
}

static pch_ccw_t blink_chanprog[] = {
        { PCH_CCW_CMD_WRITE, PCH_CCW_FLAG_CC },
        { PCH_CCW_CMD_TIC, 0, 0, (uint32_t)&blink_chanprog[0] }
};

int main(void) {
        bi_decl(bi_program_description("picochan blink CSS"));
        bi_decl(bi_4pins_with_names(
                BLINK_TX_CLOCK_IN_PIN, "TX_CLOCK_IN",
                BLINK_TX_DATA_OUT_PIN, "TX_DATA_OUT",
                BLINK_RX_CLOCK_OUT_PIN, "RX_CLOCK_OUT",
                BLINK_RX_DATA_IN_PIN, "RX_DATA_IN"));

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_css_init();
        pch_css_set_trace(BLINK_ENABLE_TRACE);
        pch_css_start(NULL, 0);

        pch_pio_config_t cfg = pch_pio_get_default_config(BLINK_PIO);
        pch_piochan_init(&cfg);

        pch_piochan_pins_t pins = {
                .tx_clock_in = BLINK_TX_CLOCK_IN_PIN,
                .tx_data_out = BLINK_TX_DATA_OUT_PIN,
                .rx_clock_out = BLINK_RX_CLOCK_OUT_PIN,
                .rx_data_in = BLINK_RX_DATA_IN_PIN
        };
        pch_piochan_config_t pc = pch_piochan_get_default_config(pins);

        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 1); // allocates SID 0 addressing UA 0
        pch_chp_set_trace(chpid, BLINK_ENABLE_TRACE);

        pch_chp_configure_piochan(chpid, &cfg, &pc);

        pch_sch_modify_enabled(0, true);
        pch_sch_modify_traced(0, BLINK_ENABLE_TRACE);

        pch_chp_start(chpid);

        pch_sch_start(0, blink_chanprog);

        while (1)
                __wfe();
}
