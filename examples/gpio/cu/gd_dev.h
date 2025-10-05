/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#ifndef _GD_DEV_H
#define _GD_DEV_H

#include "pico/time.h"

#ifndef VALUES_BUF_SIZE
#define VALUES_BUF_SIZE 16
#endif

typedef struct gd_values {
        uint16_t        count;  //!< count of valid data bytes
        uint16_t        offset; //!< current offset in data
        uint8_t         data[VALUES_BUF_SIZE];
} gd_values_t;

typedef union cfgbuf {
        gd_pins_t       pins;
        gd_filter_t     filter;
        gd_irq_t        irq;
        uint32_t        clock_period_us;
} cfgbuf_t;

typedef struct gpio_dev {
        //! cfgbuf holds a single configuration value written from
        //! the channel until it can be validated
        cfgbuf_t                cfgbuf;
        uint8_t                 cfgcmd; //!< cmd when config write in progress
        bool                    end;    //!< set when no more data available
        gd_config_t             cfg;    //!< configuration "registers"
        repeating_timer_t       rt;     //!< clocks in/out data
        gd_values_t             values; //!< current values for input/output
} gpio_dev_t;

#endif
