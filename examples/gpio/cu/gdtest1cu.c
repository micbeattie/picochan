#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "pico/binary_info.h"

#include "picochan/cu.h"

#include "gd_cu.h"
#include "gd_debug.h"
#include "uart_chan.h"

#define GDTEST1_GD_CUNUM 0

static void light_led_for_three_seconds() {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

static void gdtest1_init() {
        pch_cus_init();
        pch_cus_set_trace(true);

        uint corenum = get_core_num();
        // pch_cus_init_dma_irq_handler must be called from the core
        // on which CU interrupts and callbacks are to be handled.
        // Here, we use the DMA IRQ index number corresponding to the
        // current core number.
        uint8_t dmairqix = (uint8_t)corenum;
        dprintf("Initialising with DMA IRQ %u, core %u and CU %u\n",
                dmairqix, corenum, GDTEST1_GD_CUNUM);
        gd_cu_init(GDTEST1_GD_CUNUM, dmairqix);
        pch_cus_trace_cu(GDTEST1_GD_CUNUM, true);
	pch_cus_init_dma_irq_handler(dmairqix);
}

int main(void) {
        bi_decl(bi_program_description("picochan gpio_dev UART1 CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        gdtest1_init();

        dprintf("Initialising channel via UART%u\n", UART_NUM(uart1));
        init_uart1();
        pch_cus_uartcu_configure_default(0, uart1);
        drain_uart();
        pch_cus_cu_start(GDTEST1_GD_CUNUM);

        pch_cu_t *cu = gd_get_cu();
        for (uint i = 0; i < NUM_GPIO_DEVS; i++)
                gd_dev_init(cu, (pch_unit_addr_t)i);

        dprintf("CU %u ready\n", GDTEST1_GD_CUNUM);
        while (1)
                __wfe();
}
