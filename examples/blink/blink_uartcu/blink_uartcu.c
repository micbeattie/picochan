#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "pico/binary_info.h"

#include "picochan/cu.h"

/*
 * blink_uartcu runs the CU side of the blink Picochan example and is
 * configured to run on core 0 and serve up its "blink" device via
 * a uart channel connected to UART 1 via GPIO pins 4-7.
 * A physical connection is needed to a separate Pico that is running
 * a CSS configured to use a UART channel for that connection, such as
 * the blink_uartcss example program.
 * Although it would be possible to run that CSS side on core 1 of
 * "this" Pico (the blink_uartcu one) and have physical connections
 * between appropriate pins mapped to the Pico's two UARTs, a more
 * practical configuration would use a memory channel (memchan)
 * between the cores with no need for the UARTs or any physical
 * channel-to-CU connections - see the blink_memchan example for that.
 */

extern void blink_cu_init(pch_cuaddr_t cua);

#define CUADDR 0

#define BLINK_ENABLE_TRACE true

// Use uart1 via GPIO pins 4-7 for CU side
#define BLINK_UART uart1
#define BLINK_UART_TX_PIN 4
#define BLINK_UART_RX_PIN 5
#define BLINK_UART_CTS_PIN 6
#define BLINK_UART_RTS_PIN 7

// Baud rate for UART channel must match that used by CSS
#define BLINK_BAUDRATE 115200

static uart_inst_t *prepare_uart_gpios(void) {
        bi_decl(bi_4pins_with_func(BLINK_UART_RX_PIN,
                BLINK_UART_TX_PIN, BLINK_UART_RTS_PIN,
                BLINK_UART_CTS_PIN, GPIO_FUNC_UART));

        gpio_set_function(BLINK_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(BLINK_UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(BLINK_UART_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(BLINK_UART_RTS_PIN, GPIO_FUNC_UART);

        return BLINK_UART;
}

static void light_led_for_three_seconds(void) {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

int main(void) {
        bi_decl(bi_program_description("picochan blink CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_cus_init();
        pch_cus_set_trace(BLINK_ENABLE_TRACE);

        blink_cu_init(CUADDR);

        uart_inst_t *uart = prepare_uart_gpios();
        pch_cus_auto_configure_uartcu(CUADDR, uart, BLINK_BAUDRATE);
        pch_cu_start(CUADDR);

        while (1)
                __wfe();
}
