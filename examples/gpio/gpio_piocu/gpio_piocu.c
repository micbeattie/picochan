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

/*
 * gpio_piocu runs the CU side of the gpio Picochan example and is
 * configured to run on core 0 and serve up its "gpio" devices via
 * a PIO channel connected to PIO0 via GPIO pins 0-3.
 * A physical connection is needed to a separate Pico that is running
 * a CSS configured to use a PIO channel for that connection, such as
 * the gpio_piocss example program.
 */

#define NUM_GPIO_DEVS 8
#define FIRST_UA 0
#define CUADDR 0

#define GD_ENABLE_TRACE true

// Use PIO0 via GPIO pins 0-3 in piochan order.
#define GDCU_PIO pio0
#define GDCU_TX_CLOCK_IN_PIN   0
#define GDCU_TX_DATA_OUT_PIN   1
#define GDCU_RX_CLOCK_OUT_PIN  2
#define GDCU_RX_DATA_IN_PIN    3

static pch_cu_t gd_cu = PCH_CU_INIT(NUM_GPIO_DEVS);

static void light_led_for_three_seconds(void) {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

extern void gd_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices);

int main(void) {
        bi_decl(bi_program_description("picochan gpio_dev CU"));
        bi_decl(bi_4pins_with_names(
                GDCU_TX_CLOCK_IN_PIN, "TX_CLOCK_IN",
                GDCU_TX_DATA_OUT_PIN, "TX_DATA_OUT",
                GDCU_RX_CLOCK_OUT_PIN, "RX_CLOCK_OUT",
                GDCU_RX_DATA_IN_PIN, "RX_DATA_IN"));

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_cus_init();
        pch_cus_set_trace(GD_ENABLE_TRACE);

        gd_cu_init(&gd_cu, FIRST_UA, NUM_GPIO_DEVS);
        pch_cu_register(&gd_cu, CUADDR);
        pch_cus_trace_cu(CUADDR, GD_ENABLE_TRACE);

        pch_pio_config_t cfg = pch_pio_get_default_config(GDCU_PIO);
        pch_piochan_init(&cfg);

        pch_piochan_pins_t pins = {
                .tx_clock_in = GDCU_TX_CLOCK_IN_PIN,
                .tx_data_out = GDCU_TX_DATA_OUT_PIN,
                .rx_clock_out = GDCU_RX_CLOCK_OUT_PIN,
                .rx_data_in = GDCU_RX_DATA_IN_PIN
        };
        pch_piochan_config_t pc = pch_piochan_get_default_config(pins);

        pch_cus_piocu_configure(CUADDR, &cfg, &pc);
        pch_cu_start(CUADDR);

        while (1)
                __wfe();
}
