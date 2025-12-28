/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"

static inline dma_channel_config make_txctrl(uart_inst_t *uart, dma_channel_config ctrl) {
        channel_config_set_transfer_data_size(&ctrl, DMA_SIZE_8);
        channel_config_set_write_increment(&ctrl, false);
        uint txdreq = uart_get_dreq_num(uart, true);
        channel_config_set_dreq(&ctrl, txdreq);
        return ctrl;
}

static inline dma_channel_config make_rxctrl(uart_inst_t *uart, dma_channel_config ctrl) {
        channel_config_set_transfer_data_size(&ctrl, DMA_SIZE_8);
        channel_config_set_read_increment(&ctrl, false);
        uint rxdreq = uart_get_dreq_num(uart, false);
        channel_config_set_dreq(&ctrl, rxdreq);
        return ctrl;
}

static void init_tx(dmachan_tx_channel_t *tx, uart_inst_t *uart, pch_uartchan_config_t *cfg) {
        uint32_t hwaddr = (uint32_t)&uart_get_hw(uart)->dr; // read/write fifo
        dma_channel_config ctrl = make_txctrl(uart, cfg->ctrl);
        dmachan_1way_config_t c = dmachan_1way_config_claim(hwaddr,
                ctrl, cfg->irq_index);
        dmachan_init_tx_channel(tx, &c, &dmachan_uart_tx_channel_ops);
        dmachan_set_link_dma_irq_enabled(&tx->link, true);
}

static void init_rx(dmachan_rx_channel_t *rx, uart_inst_t *uart, pch_uartchan_config_t *cfg) {
        uint32_t hwaddr = (uint32_t)&uart_get_hw(uart)->dr; // read/write fifo
        dma_channel_config ctrl = make_rxctrl(uart, cfg->ctrl);
        dmachan_1way_config_t c = dmachan_1way_config_claim(hwaddr,
                ctrl, cfg->irq_index);
        dmachan_init_rx_channel(rx, &c, &dmachan_uart_rx_channel_ops);
        dmachan_set_link_dma_irq_enabled(&rx->link, true);
}

/*
 * baudrate must be coordinated with the other side and can be
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
void dmachan_init_uart_channel(pch_channel_t *ch, uart_inst_t *uart, pch_uartchan_config_t *cfg) {
        assert(!pch_channel_is_started(ch));
        assert(cfg->baudrate);
        uart_init(uart, cfg->baudrate);
        uart_set_hw_flow(uart, true, true);
        uart_set_format(uart, 8, 1, UART_PARITY_EVEN);
        uart_set_fifo_enabled(uart, true);
        uart_set_translate_crlf(uart, false);

        init_tx(&ch->tx, uart, cfg);
        init_rx(&ch->rx, uart, cfg);
        pch_channel_set_configured(ch, true);
}
