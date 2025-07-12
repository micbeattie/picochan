#include "hardware/gpio.h"
#include "gd_config.h"
#include "gd_pins.h"

void gd_init_out_pins(gpio_dev_t *gd) {
        gd_pins_t *p = &gd->cfg.out_pins;

        // Initialise p->count+1 GPIO pins starting at p->base
        // for output
        for (uint i = 0; i <= p->count; i++) {
                uint gpio = p->base + i;
                gpio_init(gpio);
                gpio_set_dir(gpio, GPIO_OUT);
        }
}

void gd_write_out_pins(gpio_dev_t *gd, uint8_t val) {
        gd_pins_t *p = &gd->cfg.out_pins;

        for (uint i = 0; i <= p->count; i++) {
                bool b = val & 0x01;
                gpio_put(p->base + i, b);
                val >>= 1;
        }
}

void gd_init_in_pins(gpio_dev_t *gd) {
        gd_pins_t *p = &gd->cfg.in_pins;

        // Initialise p->count+1 GPIO pins starting at p->base
        // for input
        for (uint i = 0; i <= p->count; i++) {
                uint gpio = p->base + i;
                gpio_init(gpio);
                gpio_set_dir(gpio, GPIO_IN);
        }
}

uint8_t gd_read_in_pins(gpio_dev_t *gd) {
        gd_pins_t *p = &gd->cfg.in_pins;

        uint32_t val = gpio_get_all();
        val >>= p->base; // shift down so p->base pin level is bit 0
        // Mask out bits other than bit number p->count and below
        val &= (1 << (p->count+1)) - 1;
        return val;
}
