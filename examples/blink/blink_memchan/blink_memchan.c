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

/*
 * blink_memchan runs the complete blink Picochan example on a
 * single Pico. The CSS is run on core 0 and the CU on core 1.
 * Instead of needing physical channel connections between CSS
 * and CU, this configuration uses a memory channel (memchan)
 * so that CSS-to-CU communication happens directly via
 * memory-to-memory DMA for data transfers and 4-byte
 * writes/reads from memory for command transfers.
 */

const pch_unit_addr_t FIRST_UA = 0; // First (and only) unit address
const pch_cuaddr_t CUADDR = 0;
const pch_chpid_t CHPID = 0;

#define BLINK_ENABLE_TRACE true

static pch_cu_t blink_cu = PCH_CU_INIT(1);

extern void blink_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua);

bool core1_ready;

static void core1_thread(void) {
        pch_cus_init(); // could do from core 0
        pch_cus_set_trace(BLINK_ENABLE_TRACE); // could do from core 0
        
        blink_cu_init(&blink_cu, FIRST_UA);
        pch_cu_register(&blink_cu, CUADDR);
        pch_cus_trace_cu(CUADDR, BLINK_ENABLE_TRACE);

        pch_channel_t *chpeer = pch_chp_get_channel(CHPID);
        pch_cus_memcu_configure(CUADDR, chpeer);

        pch_cu_start(CUADDR);
        core1_ready = true; // core0 waits for this

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

static pch_ccw_t blink_chanprog[] = {
        { PCH_CCW_CMD_WRITE, PCH_CCW_FLAG_CC },
        { PCH_CCW_CMD_TIC, 0, 0, (uint32_t)&blink_chanprog[0] }
};

int main(void) {
        bi_decl(bi_program_description("picochan blink memchan CSS+CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        sleep_ms(2000);

        pch_memchan_init();

        pch_css_init();
        pch_css_set_trace(BLINK_ENABLE_TRACE);
        pch_css_start(NULL, 0); // must set CSS dmairqix before this
        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 1); // allocates SID 0
        pch_chp_set_trace(chpid, BLINK_ENABLE_TRACE);

        multicore_launch_core1(core1_thread);
        while (!core1_ready)
                sleep_ms(1);

        pch_channel_t *chpeer = pch_cu_get_channel(CUADDR);
        pch_chp_configure_memchan(CHPID, chpeer);

        pch_sch_modify_enabled(0, true);
        pch_sch_modify_traced(0, BLINK_ENABLE_TRACE);

        pch_chp_start(chpid);

        pch_sch_start(0, blink_chanprog);

        while (1)
                __wfe();
}
