/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/binary_info.h"

// We use the I2C1 instance assigned to GPIO14 for SDA and
// GPIO15 for SCK.
#define CARDKB_I2C i2c1
#define CARDKB_I2C_SDA_PIN 14
#define CARDKB_I2C_SCL_PIN 15

// CardKB defaults to I2C address 0x5f and does not have a
// straightforward way to change it
#define CARDKB_I2C_ADDR 0x5F

// We may be able to use a 400KHz clock but since we are powering
// the CardKB at 3.3V instead of the 5V we are supposed to (since
// we are using Pico where we can't use 5V data for this), we drop
// the speed to 100KHz which should be fine for using this keyboard.
#define CARDKB_I2C_CLK_KHZ 100

void cardkb_i2c_init(i2c_inst_t **i2cp, uint8_t *addrp) {
        bi_decl_if_func_used(bi_2pins_with_func(CARDKB_I2C_SDA_PIN,
                CARDKB_I2C_SCL_PIN, GPIO_FUNC_I2C));
        i2c_init(CARDKB_I2C, CARDKB_I2C_CLK_KHZ * 1000);
        gpio_set_function(CARDKB_I2C_SDA_PIN, GPIO_FUNC_I2C);
        gpio_set_function(CARDKB_I2C_SCL_PIN, GPIO_FUNC_I2C);
        gpio_pull_up(CARDKB_I2C_SDA_PIN);
        gpio_pull_up(CARDKB_I2C_SCL_PIN);

        *i2cp = CARDKB_I2C;
        *addrp = CARDKB_I2C_ADDR;
}
