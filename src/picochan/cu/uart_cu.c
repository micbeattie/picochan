#include "hardware/uart.h"
#include "picochan/cu.h"

void pch_cus_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dma_channel_config ctrl) {
        pch_init_uart(uart);

        // Copy the template control register and override the fields
        // we need to. chain_to is overridden in pch_cus_init_channel
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
        uint32_t hwaddr = (uint32_t)uart_hwaddr;

        pch_cus_cu_dma_claim_and_configure(cunum, hwaddr, txctrl,
                hwaddr, rxctrl);
}

void pch_cus_uartcu_claim_and_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl) {
        pch_dmaid_t txdmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        pch_dmaid_t rxdmaid = (pch_dmaid_t)dma_claim_unused_channel(true);

        pch_cus_uartcu_configure(cunum, uart, txdmaid, rxdmaid, ctrl);
}
