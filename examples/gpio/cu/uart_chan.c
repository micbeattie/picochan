#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "uart_chan.h"

// UART definitions for CU side channel - must match
// corresponding UART on remote CSS side
uart_parity_t parity = UART_PARITY_EVEN;
uint try_baud_rate = 1200; // slow for testing
uint baud_rate_uart1 = 0;

// uart1 for CU side
#define UART1_TX_PIN 4
#define UART1_RX_PIN 5
#define UART1_CTS_PIN 6
#define UART1_RTS_PIN 7

void init_uart1(void) {
        bi_decl_if_func_used(bi_4pins_with_func(UART1_RX_PIN,
                UART1_TX_PIN, UART1_RTS_PIN, UART1_CTS_PIN,
                GPIO_FUNC_UART));
        gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(UART1_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(UART1_RTS_PIN, GPIO_FUNC_UART);
        baud_rate_uart1 = uart_init(uart1, try_baud_rate);
}

uint32_t drain_uart1 = 0;

// drain_uart drains the rx fifos since, at least on the CSS side,
// the order/way we initialise the UARTs and/or DMA seems to mean its
// rx fifo starts with a \0 byte which draining manually shows, with
// upper bits, is 0x0500 meaning Break Error and Framing Error. Maybe
// it's just the initial period with the tx low gets counted as a
// Break condition. We'll put the drain values in global variables
// so we can inspect with gdb easily.
void drain_uart() {
        drain_uart1 = uart_get_hw(uart1)->dr;
}
