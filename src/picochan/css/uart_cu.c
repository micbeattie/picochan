#include "hardware/uart.h"
#include "picochan/css.h"

// pch_css_init_uart initialises uart as needed for CSS<->CU:
// 8 data bits, 1 stop bit, even parity (although these three settings
// are simply for interoperability between CSS and CU) and RTS and CTS
// flow control are enabled (which is absolutely mandatory because of
// the way we use DMA to handle the communication).
void pch_css_init_uart(uart_inst_t *uart) {
        uart_set_hw_flow(uart, true, true);
        uart_set_format(uart, 8, 1, UART_PARITY_EVEN);
        uart_set_fifo_enabled(uart, true);
        uart_set_translate_crlf(uart, false);
}

// pch_css_register_uart_cu initialises uart with pch_css_init_uart,
// prepares two appropriate DMA control registers for inbound and
// outbound communication via that uart (disabled increment for the
// uart-side, 8-bit-sized transfers and the DREQs appropriate for the
// uart) then calls pch_css_register_cu to register uart as the
// CSS-side of the channel to the partner CU.
void pch_css_register_uart_cu(pch_cunum_t cunum, uint16_t num_devices, uart_inst_t *uart, dma_channel_config ctrl) {
        pch_css_init_uart(uart);

        // Copy the template control register and override the fields
        // we need to. chain_to is overridden in pch_css_register_cu
        dma_channel_config txctrl = ctrl;
        channel_config_set_transfer_data_size(&txctrl, DMA_SIZE_8);
        channel_config_set_write_increment(&txctrl, false);
        uint txdreq = uart_get_dreq_num(uart, true);
        channel_config_set_dreq(&txctrl, txdreq);

        dma_channel_config rxctrl = ctrl;
        channel_config_set_transfer_data_size(&rxctrl, DMA_SIZE_8);
        channel_config_set_read_increment(&rxctrl, false);
        uint rxdreq = uart_get_dreq_num(uart, false);
        channel_config_set_dreq(&rxctrl, rxdreq);

        io_rw_32 *uart_hwaddr = &uart_get_hw(uart)->dr; // read/write fifo
        pch_css_register_cu(cunum, num_devices,
                (uint32_t)uart_hwaddr, txctrl,
                (uint32_t)uart_hwaddr, rxctrl);
}

