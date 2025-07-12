#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "pico/time.h"
#include "pico/binary_info.h"

#include "picochan/cu.h"

#include "gd_cu.h"
#include "uart_chan.h"

uint8_t dmairqix_cu = 0; // Use DMA_IRQ_0 for CU
bool default_trace_cu = true;

static void gdtest1_init() {
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);

        pch_cus_init();
        pch_cus_set_trace(true);
        
        gd_cu_init(0, dmairqix_cu);
        pch_cus_trace_cu(0, default_trace_cu);

        // pch_cus_init_dma_irq_handler must be called from the core
        // on which CU interrupts and callbacks are to be handled
	pch_cus_init_dma_irq_handler(dmairqix_cu);
}

int main(void) {
        bi_decl(bi_program_description("picochan gpio_dev UART1 CU"));
        gdtest1_init();

        init_uart1();
        dma_channel_config ctrl = dma_channel_get_default_config(0);
        pch_cus_uartcu_configure(0, uart1, ctrl);
        drain_uart();
        pch_cus_cu_start(0);

        pch_cu_t *cu = gd_get_cu();
        gd_dev_init(cu, 0); // UA0
        gd_dev_init(cu, 1); // UA1
        gd_dev_init(cu, 2); // UA2

        while (1)
                best_effort_wfe_or_timeout(at_the_end_of_time);
}
