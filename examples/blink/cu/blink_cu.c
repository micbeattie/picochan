/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "picochan/cu.h"
#include "picochan/dev_status.h"
#include "picochan/ccw.h"

/*
 * blink_cu implements a CU for a "blink" device.
 * A channel program (running on a Picochan CSS instance)
 * which issues a plain "WRITE" CCW to this device causes
 * this driver to toggle the LED on/off then, after 250ms
 * (LED_DELAY_MS milliseconds), send an UpdateStatus to the
 * CSS side for it to continue or complete the channel program.
 * This blink_cu source file can be used from any CU-side
 * program that calls blink_cu_init() to initialise this CU,
 * for example blink_uartcu (which serves up this driver via
 * a physical UART connection to a separate Pico running a
 * CSS) or blink_memchan (which has both the CU-side and the
 * CSS-side running on the same Pico on separate cores with
 * no physical connections needed).
 */

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

#ifndef PICO_DEFAULT_LED_PIN
#warning blink_cu requires a board with a regular LED
#endif

static pch_dev_range_t blink_dev_range;
static alarm_pool_t *alarm_pool;
static repeating_timer_t timer;
static pch_cbindex_t start_cbindex;

static bool timer_callback(repeating_timer_t *rt) {
        pch_devib_t *devib = rt->user_data;
        pch_dev_update_status_ok_then(devib, start_cbindex);

        return false; // stop repeating timer
}

static int do_start(pch_devib_t *devib) {
        uint8_t ccwcmd = devib->payload.p0;
        if (ccwcmd != PCH_CCW_CMD_WRITE)
                return -EINVALIDCMD;

        gpio_xor_mask(1U << PICO_DEFAULT_LED_PIN);

        alarm_pool_add_repeating_timer_ms(alarm_pool,
                -LED_DELAY_MS, timer_callback, devib, &timer);

        return 0;
}

static void start(pch_devib_t *devib) {
        pch_dev_call_or_reject_then(devib, do_start, start_cbindex);
}

void blink_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua) {
        pch_dev_range_init(&blink_dev_range, cu, first_ua, 1);

        start_cbindex = pch_register_unused_devib_callback(start, NULL);

        alarm_pool = alarm_pool_create_with_unused_hardware_alarm(1);

        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

        pch_dev_range_set_callback(&blink_dev_range, start_cbindex);
}
