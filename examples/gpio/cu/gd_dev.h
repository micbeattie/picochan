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

// CFGBUF_SIZE must be large enough to hold a copy of the largest
// configuration register that can be written with setconf and
// must be appropriately aligned in gpio_dev to hold the
// associated value. Currently the uint32_t for clock_period_us
// is both the largest and the one requiring the highest alignment.
// Since the cfgbuf member follows a pointer, it will already be
// sufficiently aligned for this.
#define CFGBUF_SIZE 4

typedef struct gpio_dev {
        //! cfgbuf holds a configuration value written from the channel
        //! until it can be validated
        unsigned char           cfgbuf[CFGBUF_SIZE];
        uint8_t                 cfgcmd; //!< cmd when config write in progress
        gd_config_t             cfg;    //!< configuration "registers"
        repeating_timer_t       rt;     //!< clocks in/out data
        gd_values_t             values; //!< current values for input/output
} gpio_dev_t;

#endif
