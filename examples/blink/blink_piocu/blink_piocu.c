/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "pico/binary_info.h"

#include "picochan/cu.h"

// First (and only) unit address
#define FIRST_UA 0

/*
 * blink_piocu runs the CU side of the blink Picochan example and is
 * configured to run on core 0 and serve up its "blink" device via
 * a PIO channel connected to PIO0 via GPIO pins 0-3.
 * A physical connection is needed to a separate Pico that is running
 * a CSS configured to use a PIO channel for that connection, such as
 * the blink_piocss example program.
 */

#define CUADDR 0

#define BLINK_ENABLE_TRACE true

// Use PIO0 via GPIO pins 0-3 in piochan order.
#define BLINK_PIO pio0
#define BLINK_TX_CLOCK_IN_PIN   0
#define BLINK_TX_DATA_OUT_PIN   1
#define BLINK_RX_CLOCK_OUT_PIN  2
#define BLINK_RX_DATA_IN_PIN    3

static pch_cu_t blink_cu = PCH_CU_INIT(1);

static void light_led_for_three_seconds(void) {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

extern void blink_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua);

int main(void) {
        bi_decl(bi_program_description("picochan blink CU"));
        bi_decl(bi_4pins_with_names(
                BLINK_TX_CLOCK_IN_PIN, "TX_CLOCK_IN",
                BLINK_TX_DATA_OUT_PIN, "TX_DATA_OUT",
                BLINK_RX_CLOCK_OUT_PIN, "RX_CLOCK_OUT",
                BLINK_RX_DATA_IN_PIN, "RX_DATA_IN"));

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_cus_init();
        pch_cus_set_trace(BLINK_ENABLE_TRACE);

        blink_cu_init(&blink_cu, FIRST_UA);
        pch_cu_register(&blink_cu, CUADDR);
        pch_cus_trace_cu(CUADDR, BLINK_ENABLE_TRACE);

        pch_pio_config_t cfg = pch_pio_get_default_config(BLINK_PIO);
        pch_piochan_init(&cfg);

        pch_piochan_pins_t pins = {
                .tx_clock_in = BLINK_TX_CLOCK_IN_PIN,
                .tx_data_out = BLINK_TX_DATA_OUT_PIN,
                .rx_clock_out = BLINK_RX_CLOCK_OUT_PIN,
                .rx_data_in = BLINK_RX_DATA_IN_PIN
        };
        pch_piochan_config_t pc = pch_piochan_get_default_config(pins);

        pch_cus_piocu_configure(CUADDR, &cfg, &pc);
        pch_cu_start(CUADDR);

        while (1)
                __wfe();
}
