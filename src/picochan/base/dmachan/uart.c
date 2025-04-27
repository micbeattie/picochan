/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "picochan/dmachan.h"

// pch_init_uart is a simple convenience function to initialise uart
// as needed for CSS<->CU:
//   * 8 data bits, 1 stop bit, even parity - these three settings
//     are simply so that CSS and CU can interoperate when both
//     initialised using this function)
//   * crlf translation disabled (we use 8-bit binary data)
//   * RTS and CTS flow control are enabled - this is absolutely
//     mandatory because of the way we use DMA and rely on the
//     uart flow control to handle blocking automatically
void pch_init_uart(uart_inst_t *uart) {
        uart_set_hw_flow(uart, true, true);
        uart_set_format(uart, 8, 1, UART_PARITY_EVEN);
        uart_set_fifo_enabled(uart, true);
        uart_set_translate_crlf(uart, false);
}
