#ifndef _GD_PINS_H
#define _GD_PINS_H

#include "gd_dev.h"

#ifndef GD_ENABLE_GPIO_WRITES
#define GD_ENABLE_GPIO_WRITES 1
#endif

#ifndef GD_ENABLE_GPIO_VERBOSE
#define GD_ENABLE_GPIO_VERBOSE 0
#endif

void gd_init_out_pins(gpio_dev_t *gd);
void gd_write_out_pins(gpio_dev_t *gd, uint8_t val);
void gd_init_in_pins(gpio_dev_t *gd);
uint8_t gd_read_in_pins(gpio_dev_t *gd);

#endif
