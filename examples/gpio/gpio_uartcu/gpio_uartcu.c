#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "pico/binary_info.h"

#include "picochan/cu.h"

extern void gd_cu_init(pch_cuaddr_t cua);

#define CUADDR 0

#define GD_ENABLE_TRACE true

// Use uart1 via GPIO pins 4-7 for CU side
#define GDCU_UART uart1
#define GDCU_UART_TX_PIN 4
#define GDCU_UART_RX_PIN 5
#define GDCU_UART_CTS_PIN 6
#define GDCU_UART_RTS_PIN 7

// Baud rate for UART channel must match that used by CSS
#define GD_BAUDRATE 115200

static uart_inst_t *prepare_uart_gpios(void) {
        bi_decl(bi_4pins_with_func(GDCU_UART_RX_PIN,
                GDCU_UART_TX_PIN, GDCU_UART_RTS_PIN,
                GDCU_UART_CTS_PIN, GPIO_FUNC_UART));

        gpio_set_function(GDCU_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCU_UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCU_UART_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDCU_UART_RTS_PIN, GPIO_FUNC_UART);

        return GDCU_UART;
}

static void light_led_for_three_seconds(void) {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

int main(void) {
        bi_decl(bi_program_description("picochan gpio_dev CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_cus_init();
        pch_cus_set_trace(GD_ENABLE_TRACE);

        gd_cu_init(CUADDR);

        uart_inst_t *uart = prepare_uart_gpios();
        pch_cus_auto_configure_uartcu(CUADDR, uart, GD_BAUDRATE);
        pch_cu_start(CUADDR);

        while (1)
                __wfe();
}
