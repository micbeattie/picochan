#ifndef _GD_CONFIG_H
#define _GD_CONFIG_H

#include "../gd_api.h"

typedef struct gd_config {
        //! count of microseconds for the delay between each
        //! successive read from input pins or write to output pins
        uint32_t        clock_period_us;
        //! consecutive range of 1-8 GPIO output pins
        gd_pins_t       out_pins;
        //! consecutive range of 1-8 GPIO input pins
        gd_pins_t       in_pins;
        //! filter condition tested by TEST and (optionally) irq
        gd_filter_t     filter;
        //! GPIO pin configuration to trigger unsolicited device
        //! Attention or, during channel program, UnitException
        gd_irq_t        irq;
} gd_config_t;

#endif
