#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "pico/binary_info.h"

#include "picochan/cu.h"

#include "gd_cu.h"
#include "../gd_debug.h"
#include "../gd_channel.h"

#define GDCU_NUM 0

// Use uart1 via GPIO pins 4-7 for CU side
#define GDCU_UART_NUM 1
#define GDCU_UART_TX_PIN 4
#define GDCU_UART_RX_PIN 5
#define GDCU_UART_CTS_PIN 6
#define GDCU_UART_RTS_PIN 7

static uart_inst_t *prepare_uart_gpios(void) {
        bi_decl(bi_4pins_with_func(GDCU_UART_RX_PIN,
                GDCU_UART_TX_PIN, GDCU_UART_RTS_PIN,
                GDCU_UART_CTS_PIN, GPIO_FUNC_UART));

        gpio_set_function(GDCU_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCU_UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCU_UART_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCU_UART_RTS_PIN, GPIO_FUNC_UART);

        return UART_INSTANCE(GDCU_UART_NUM);
}

static void light_led_for_three_seconds(void) {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

void *volatile discard;

int main(void) {
        bi_decl(bi_program_description("picochan gpio_dev CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();
        light_led_for_three_seconds();

        uint corenum = get_core_num();
        uint8_t dmairqix = (uint8_t)corenum;
        dprintf("Initialising CU side: core %u, DMA IRQ index %u\n",
                corenum, dmairqix);
        pch_cus_init();
        pch_cus_set_trace((bool)GD_ENABLE_TRACE);
	pch_cus_init_dma_irq_handler(dmairqix);

        dprintf("Initialising CU %u as gpio_dev CU\n", GDCU_NUM);
        gd_cu_init(GDCU_NUM, dmairqix);

        uart_inst_t *uart = prepare_uart_gpios();
        dprintf("Configuring channel via UART%u for CU %u\n",
                UART_NUM(uart), GDCU_NUM);
        pch_cus_auto_configure_uartcu(GDCU_NUM, uart, BAUDRATE);

        pch_cu_t *cu = gd_get_cu();
        dprintf("Initialising %u gpio_dev devices\n", NUM_GPIO_DEVS);
        for (uint i = 0; i < NUM_GPIO_DEVS; i++) {
                pch_devib_t *devib = pch_get_devib(cu, i);
                gd_dev_init(devib);
        }

        dprintf("Starting CU %u\n", GDCU_NUM);
        pch_cus_cu_start(GDCU_NUM);
        dprintf("CU %u is ready\n", GDCU_NUM);

        while (1)
                __wfe();
}
