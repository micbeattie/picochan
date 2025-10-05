/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"

#include "picochan/css.h"

#include "../gd_api.h"

#define GD_ENABLE_TRACE true

#define NUM_GPIO_DEVS      8

// Use uart0 via GPIO pins 0-3 for CSS side
#define GDCSS_UART uart0
#define GDCSS_UART_TX_PIN 0
#define GDCSS_UART_RX_PIN 1
#define GDCSS_UART_CTS_PIN 2
#define GDCSS_UART_RTS_PIN 3

// Baud rate for UART channel must match that used by CU
#define GD_BAUDRATE 115200

static uart_inst_t *prepare_uart_gpios(void) {
        bi_decl_if_func_used(bi_4pins_with_func(GDCSS_UART_RX_PIN,
                GDCSS_UART_TX_PIN, GDCSS_UART_RTS_PIN,
                GDCSS_UART_CTS_PIN, GPIO_FUNC_UART));
        gpio_set_function(GDCSS_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCSS_UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCSS_UART_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCSS_UART_RTS_PIN, GPIO_FUNC_UART);

        return GDCSS_UART;
}

static void light_led_for_three_seconds() {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

static gd_pins_t led_pins = {
        .base = PICO_DEFAULT_LED_PIN,
        .count = 0      // .count+1 = 1 pin starting from PICO_DEFAULT_LED_PIN
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

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_css_init();
        pch_css_set_trace(GD_ENABLE_TRACE);
        pch_css_start(NULL, 0);

        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_sid_t first_sid = pch_chp_alloc(chpid, NUM_GPIO_DEVS);

        uart_inst_t *uart = prepare_uart_gpios();
        pch_chp_auto_configure_uartchan(chpid, uart, GD_BAUDRATE);
        pch_chp_set_trace(chpid, GD_ENABLE_TRACE);

        pch_sch_modify_enabled_range(first_sid, NUM_GPIO_DEVS, true);
        pch_sch_modify_traced_range(first_sid, NUM_GPIO_DEVS, true);

        pch_chp_start(chpid);

        pch_sch_start(0, led_chanprog);

        while (1)
                __wfe();
}
