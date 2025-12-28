/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"

#include "picochan/css.h"
#include "picochan/cu.h"

#include "../gd_api.h"

#define NUM_GPIO_DEVS 8
#define FIRST_UA 0

#define GD_ENABLE_TRACE true

const pch_cuaddr_t CUADDR = 0;
const pch_chpid_t CHPID = 0;

/*
 * gpio_memchan runs the complete gpio_dev Picochan example on a
 * single Pico. The CSS is run on core 0 and the CU on core 1.
 * Instead of needing physical channel connections between CSS
 * and CU, this configuration uses a memory channel (memchan)
 * so that CSS-to-CU communication happens directly via
 * memory-to-memory DMA for data transfers and 4-byte
 * writes/reads from memory for command transfers.
 */

static pch_cu_t gd_cu = PCH_CU_INIT(NUM_GPIO_DEVS);

pch_dmaid_t css_to_cu_dmaid;
pch_dmaid_t cu_to_css_dmaid;
pch_dma_irq_index_t css_dmairqix = -1;
pch_dma_irq_index_t cu_dmairqix = -1;

extern void gd_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices);

static void core1_thread(void) {
        pch_cus_init();
        pch_cus_set_trace(GD_ENABLE_TRACE);
        pch_cus_configure_dma_irq_index_shared_default(cu_dmairqix);

        gd_cu_init(&gd_cu, FIRST_UA, NUM_GPIO_DEVS); // must call from core 1
        pch_cu_register(&gd_cu, CUADDR);
        pch_cus_trace_cu(CUADDR, GD_ENABLE_TRACE);

        pch_channel_t *chpeer = pch_chp_get_channel(CHPID);
        pch_cus_memcu_configure(CUADDR, cu_to_css_dmaid,
                css_to_cu_dmaid, chpeer);

        pch_cu_start(CUADDR);

        while (1)
                __wfe();
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
        bi_decl(bi_program_description("picochan gd_cu test memchan CSS+CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        css_to_cu_dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        cu_to_css_dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        css_dmairqix = 0;
        cu_dmairqix = 1;

        pch_memchan_init();

        pch_css_init();
        pch_css_set_trace(GD_ENABLE_TRACE);
        pch_css_configure_dma_irq_index_shared(css_dmairqix,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        pch_css_start(NULL, 0); // must set CSS dmairqix before this
        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 1); // allocates SID 0
        pch_chp_set_trace(chpid, GD_ENABLE_TRACE);

        multicore_launch_core1(core1_thread);
        sleep_ms(2000); // XXX not sure if there's a race

        pch_channel_t *chpeer = pch_cu_get_channel(CUADDR);
        pch_chp_configure_memchan(CHPID, css_to_cu_dmaid,
                cu_to_css_dmaid, chpeer);

        pch_sch_modify_enabled(0, true);
        pch_sch_modify_traced(0, GD_ENABLE_TRACE);

        pch_chp_start(chpid);

        pch_sch_start(0, led_chanprog);

        while (1)
                __wfe();
}
