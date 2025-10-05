/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"
#include "pico/stdio.h"

#include "picochan/css.h"
#include "picochan/dev_status.h"

#include "../cardkb_api.h"

#define CARDKB_ENABLE_TRACE true

// Use uart0 via GPIO pins 0-3 for CSS side
#define CARDKB_UART uart0
#define CARDKB_UART_TX_PIN 0
#define CARDKB_UART_RX_PIN 1
#define CARDKB_UART_CTS_PIN 2
#define CARDKB_UART_RTS_PIN 3

// Baud rate for UART channel must match that used by CU
#define CARDKB_BAUDRATE 115200

static uart_inst_t *prepare_uart_gpios(void) {
        bi_decl_if_func_used(bi_4pins_with_func(CARDKB_UART_RX_PIN,
                CARDKB_UART_TX_PIN, CARDKB_UART_RTS_PIN,
                CARDKB_UART_CTS_PIN, GPIO_FUNC_UART));
        gpio_set_function(CARDKB_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(CARDKB_UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(CARDKB_UART_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(CARDKB_UART_RTS_PIN, GPIO_FUNC_UART);

        return CARDKB_UART;
}

static void light_led_for_three_seconds() {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

static char buff[64];

static cardkb_dev_config_t cdc = {
        .timeout_cs = 0xffff,   // never timeout
        .eol = '\r',            // end when Enter key pressed (yes, \r)...
        .minread = 0xff         // ...and not before
};

static pch_ccw_t configure_kb_prog[] = {
        { CARDKB_CCW_CMD_SET_CONFIG, 0, sizeof(cdc), (uint32_t)&cdc }
};

static pch_ccw_t read_line_from_kb_prog[] = {
        { PCH_CCW_CMD_READ, PCH_CCW_FLAG_SLI,
                sizeof(buff)-1, (uint32_t)&buff }
};

static void read_and_print_line() {
        pch_scsw_t scsw;

        puts("Type some keys on the CardKB, ending with Enter (<-')");
        pch_sch_run_wait(0, read_line_from_kb_prog, &scsw);
        // CCW count was sizeof(buff)-1
        uint16_t rescount = scsw.count;
        assert(rescount < sizeof(buff));
        uint n = sizeof(buff) - 1 - rescount; // 0...sizeof(buff)-1
        buff[n] = 0;
        printf("You typed: %s\n", buff);
}

int main(void) {
        bi_decl(bi_program_description("picochan cardkb_dev test1 UART0 CSS"));

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();
        light_led_for_three_seconds();

        puts("Starting...");
        pch_css_init();
        pch_css_set_trace(CARDKB_ENABLE_TRACE);
        pch_css_start(NULL, 0);

        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 1); // Allocates SID 0

        uart_inst_t *uart = prepare_uart_gpios();
        pch_chp_auto_configure_uartchan(chpid, uart, CARDKB_BAUDRATE);
        pch_chp_set_trace(chpid, CARDKB_ENABLE_TRACE);

        pch_sch_modify_enabled(0, true);
        pch_sch_modify_traced(0, true);

        pch_chp_start(chpid);

        pch_scsw_t scsw;
        pch_sch_run_wait(0, configure_kb_prog, &scsw);

        while (1)
                read_and_print_line();
}
