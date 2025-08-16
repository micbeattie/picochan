#include "hardware/gpio.h"
#include "gd_config.h"
#include "gd_pins.h"

void gd_init_out_pins(gpio_dev_t *gd) {
        gd_pins_t *p = &gd->cfg.out_pins;

        // Initialise p->count+1 GPIO pins starting at p->base
        // for output
        for (uint i = 0; i <= p->count; i++) {
                uint gpio = p->base + i;
                if (gpio > GD_MAX_PIN)
                        break;

                if (GD_IGNORE_GPIO_WRITE_MASK & (1u << gpio))
                        continue;

                gpio_init(gpio);
                gpio_set_dir(gpio, GPIO_OUT);
        }
}

void gd_write_out_pins(gpio_dev_t *gd, uint8_t val) {
        gd_pins_t *p = &gd->cfg.out_pins;

        // We process p->count+1 pins.
        uint32_t mask = ((1u << (p->count + 1)) - 1) << p->base;
        mask &= ~GD_IGNORE_GPIO_WRITE_MASK;
        uint32_t value_bits = (uint32_t)val << p->base;
        gpio_put_masked(mask, value_bits);
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
