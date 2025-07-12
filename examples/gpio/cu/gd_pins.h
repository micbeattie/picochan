#ifndef _GD_PINS_H
#define _GD_PINS_H

#include "gd_dev.h"

void gd_init_out_pins(gpio_dev_t *gd);
void gd_write_out_pins(gpio_dev_t *gd, uint8_t val);
void gd_init_in_pins(gpio_dev_t *gd);
uint8_t gd_read_in_pins(gpio_dev_t *gd);

#endif
