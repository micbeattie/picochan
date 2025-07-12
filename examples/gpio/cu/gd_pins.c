#include "hardware/gpio.h"
#include "gd_config.h"
#include "gd_pins.h"
#if GD_ENABLE_GPIO_VERBOSE
#include "stdio.h"
#endif

void gd_init_out_pins(gpio_dev_t *gd) {
        gd_pins_t *p = &gd->cfg.out_pins;

        // Initialise p->count+1 GPIO pins starting at p->base
        // for output
#if GD_ENABLE_GPIO_VERBOSE
        printf("init GPIO out %d..%d\n", p->base, p->base + p->count);
#endif
#if GD_ENABLE_GPIO_WRITES
        for (uint i = 0; i <= p->count; i++) {
                uint gpio = p->base + i;
                gpio_init(gpio);
                gpio_set_dir(gpio, GPIO_OUT);
        }
#endif
}

void gd_write_out_pins(gpio_dev_t *gd, uint8_t val) {
        gd_pins_t *p = &gd->cfg.out_pins;

#if GD_ENABLE_GPIO_VERBOSE
        printf("GPIO write %d..%d: 0x%02x\n",
                p->base, p->base + p->count, (unsigned int)val);
#endif
#if GD_ENABLE_GPIO_WRITES
        for (uint i = 0; i <= p->count; i++) {
                bool b = val & 0x01;
                gpio_put(p->base + i, b);
                val >>= 1;
        }
#endif
}

void gd_init_in_pins(gpio_dev_t *gd) {
        gd_pins_t *p = &gd->cfg.in_pins;

        // Initialise p->count+1 GPIO pins starting at p->base
        // for input
#if GD_ENABLE_GPIO_VERBOSE
        printf("init GPIO in %d..%d\n", p->base, p->base + p->count);
#endif
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
#if GD_ENABLE_GPIO_VERBOSE
        printf("GPIO read %d..%d: 0x%02x\n",
                p->base, p->base + p->count, (unsigned int)val);
#endif
        return val;
}
