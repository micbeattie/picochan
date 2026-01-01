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
 * gpio_piocss runs the CSS side of the gpio Picochan example and is
 * configured to run on core 0 and connect to a gpio CU instance via
 * a PIO channel connected to PIO0 via GPIO pins 0-3.
 * A physical connection is needed to a separate Pico that is hosting
 * a PIO CU via that connection with a gpio device on unit address 0,
 * such as the gpio_piocu example program.
 * The example remotely controls a GPIO pin on the CU, expected to be
 * connected to an LED, to blink it in a pattern. If the CU does not
 * have GPIO 25 connected to an LED then change CU_LED_PIN below.
 *
 * If the CU is running on a board without a direct GPIO-driven LED or
 * equivalent (such as a Pico W or Pico 2W where the on-board LED is
 * connected indirectly via the cyw3-driven WiFi chop) then this
 * example cannot drive it since the CU serves up access only to its
 * direct GPIO pins.
 */

#define CU_LED_PIN 25

#include "../gd_api.h"

#define GD_ENABLE_TRACE true

#define NUM_GPIO_DEVS      8

// Use PIO0 via GPIO pins 0-3 in piochan order.
#define GD_PIO pio0
#define GD_TX_CLOCK_IN_PIN   0
#define GD_TX_DATA_OUT_PIN   1
#define GD_RX_CLOCK_OUT_PIN  2
#define GD_RX_DATA_IN_PIN    3

static void light_led_for_three_seconds() {
        status_led_init();
        status_led_set_state(true);
        sleep_ms(3000);
        status_led_set_state(false);
}

static gd_pins_t led_pins = {
        .base = CU_LED_PIN,
        .count = 0      // .count+1 = 1 pin starting from CU_LED_PIN
};

static uint32_t led_clock_period_us = 250000; // 250ms

static uint8_t led_data[] = {
        1, 0, 0, 0, 0, 0, 0, 0, // one flash
        1, 0, 1, 0, 0, 0, 0, 0, // two flashes
        1, 0, 1, 0, 1, 0, 0, 0, // three flashes
        0, 0, 0, 0, 0, 0, 0, 0  // a two second gap
};

static pch_ccw_t led_chanprog[] = {
        { GD_CCW_CMD_SET_OUT_PINS, PCH_CCW_FLAG_CC,
                sizeof(led_pins), (uint32_t)&led_pins },
        { GD_CCW_CMD_SET_CLOCK_PERIOD_US, PCH_CCW_FLAG_CC,
                sizeof(led_clock_period_us), (uint32_t)&led_clock_period_us },
        // Next is CCW 2 which is where we loop (TIC) back to...
        { PCH_CCW_CMD_WRITE, PCH_CCW_FLAG_CC,
                sizeof(led_data), (uint32_t)led_data },
        // ...here
        { PCH_CCW_CMD_TIC, 0, 0, (uint32_t)&led_chanprog[2] }
};

int main(void) {
        bi_decl(bi_program_description("picochan gpio_dev test1 UART0 CSS"));
        bi_decl(bi_4pins_with_names(
                GD_TX_CLOCK_IN_PIN, "TX_CLOCK_IN",
                GD_TX_DATA_OUT_PIN, "TX_DATA_OUT",
                GD_RX_CLOCK_OUT_PIN, "RX_CLOCK_OUT",
                GD_RX_DATA_IN_PIN, "RX_DATA_IN"));

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_css_init();
        pch_css_set_trace(GD_ENABLE_TRACE);
        pch_css_start(NULL, 0);

        pch_pio_config_t cfg = pch_pio_get_default_config(GD_PIO);
        pch_piochan_init(&cfg);

        pch_piochan_pins_t pins = {
                .tx_clock_in = GD_TX_CLOCK_IN_PIN,
                .tx_data_out = GD_TX_DATA_OUT_PIN,
                .rx_clock_out = GD_RX_CLOCK_OUT_PIN,
                .rx_data_in = GD_RX_DATA_IN_PIN
        };
        pch_piochan_config_t pc = pch_piochan_get_default_config(pins);

        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_sid_t first_sid = pch_chp_alloc(chpid, NUM_GPIO_DEVS);
        pch_chp_set_trace(chpid, GD_ENABLE_TRACE);
        pch_chp_configure_piochan(chpid, &cfg, &pc);

        pch_sch_modify_enabled_range(first_sid, NUM_GPIO_DEVS, true);
        pch_sch_modify_traced_range(first_sid, NUM_GPIO_DEVS, true);

        pch_chp_start(chpid);

        pch_sch_start(0, led_chanprog);

        while (1)
                __wfe();
}
