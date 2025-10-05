/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"

#include "picochan/css.h"

#define BLINK_ENABLE_TRACE true

// Use uart0 via GPIO pins 0-3 for CSS side
#define BLINK_UART uart0
#define BLINK_UART_TX_PIN 0
#define BLINK_UART_RX_PIN 1
#define BLINK_UART_CTS_PIN 2
#define BLINK_UART_RTS_PIN 3

// Baud rate for UART channel must match that used by CU
#define BLINK_BAUDRATE 115200

static uart_inst_t *prepare_uart_gpios(void) {
        bi_decl_if_func_used(bi_4pins_with_func(BLINK_UART_RX_PIN,
                BLINK_UART_TX_PIN, BLINK_UART_RTS_PIN,
                BLINK_UART_CTS_PIN, GPIO_FUNC_UART));

        gpio_set_function(BLINK_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(BLINK_UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(BLINK_UART_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(BLINK_UART_RTS_PIN, GPIO_FUNC_UART);

        return BLINK_UART;
}

static void light_led_for_three_seconds() {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

static pch_ccw_t blink_chanprog[] = {
        { PCH_CCW_CMD_WRITE, PCH_CCW_FLAG_CC },
        { PCH_CCW_CMD_TIC, 0, 0, (uint32_t)&blink_chanprog[0] }
};

int main(void) {
        bi_decl(bi_program_description("picochan blink CSS"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_css_init();
        pch_css_set_trace(BLINK_ENABLE_TRACE);
        pch_css_start(NULL, 0);

        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 1); // allocates SID 0

        uart_inst_t *uart = prepare_uart_gpios();
        pch_chp_auto_configure_uartchan(chpid, uart, BLINK_BAUDRATE);
        pch_chp_set_trace(chpid, BLINK_ENABLE_TRACE);

        pch_sch_modify_enabled(0, true);
        pch_sch_modify_traced(0, BLINK_ENABLE_TRACE);

        pch_chp_start(chpid);

        pch_sch_start(0, blink_chanprog);

        while (1)
                __wfe();
}
