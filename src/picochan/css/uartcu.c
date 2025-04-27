#include "picochan/css.h"

void pch_css_uartcu_dma_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl) {
        pch_init_uart(uart);

        dma_channel_config txctrl = dmachan_uartcu_make_txctrl(uart, ctrl);
        dma_channel_config rxctrl = dmachan_uartcu_make_rxctrl(uart, ctrl);
        uint32_t hwaddr = (uint32_t)&uart_get_hw(uart)->dr; // read/write fifo
        dmachan_config_t dc = dmachan_config_claim(hwaddr, txctrl,
                hwaddr, rxctrl);
        pch_css_cu_dma_configure(cunum, &dc);
}
