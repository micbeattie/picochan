/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "picochan/dmachan.h"

/*! \brief pch_uart_init is a convenience function to initialise
 *  either side of a CSS<->CU channel
 * \ ingroup picochan_base
 *
 * \param baudrate must be coordinated with the other side and can be
 * anything reasonable. In addition to setting the baudrate on the
 * uart, the function sets:
 *   * 8 data bits, 1 stop bit, even parity - these three settings
 *     are simply so that CSS and CU can interoperate when both
 *     initialised using this function)
 *   * crlf translation disabled (we use 8-bit binary data)
 *   * RTS and CTS flow control are enabled - this is absolutely
 *     mandatory because of the way we use DMA and rely on the
 *     uart flow control to handle blocking automatically
 */
void pch_uart_init(uart_inst_t *uart, uint baudrate) {
        uart_init(uart, baudrate);
        uart_set_hw_flow(uart, true, true);
        uart_set_format(uart, 8, 1, UART_PARITY_EVEN);
        uart_set_fifo_enabled(uart, true);
        uart_set_translate_crlf(uart, false);
}
