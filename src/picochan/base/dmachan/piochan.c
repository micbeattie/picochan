/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "dmachan_internal.h"
#include "piochan.pio.h"

static inline void trace_piochan_init(pch_channel_t *ch, uint8_t id, pch_pio_config_t *cfg, pch_piochan_config_t *pc) {
        PCH_DMACHAN_LINK_TRACE(PCH_TRC_RT_DMACHAN_PIOCHAN_INIT,
                &ch->tx.link, ((struct pch_trdata_dmachan_piochan_init){
                        .id = id,
                        .pio_num = (uint8_t)PIO_NUM(cfg->pio),
                        .irq_index = cfg->irq_index,
                        .tx_sm = (uint8_t)pc->tx_sm,
                        .rx_sm = (uint8_t)pc->rx_sm,
                        .tx_offset = (uint8_t)cfg->tx_offset,
                        .rx_offset = (uint8_t)cfg->rx_offset,
                        .tx_clock_in = pc->pins.tx_clock_in,
                        .tx_data_out = pc->pins.tx_data_out,
                        .rx_clock_out = pc->pins.rx_clock_out,
                        .rx_data_in = pc->pins.rx_data_in
        }));
}

static inline dma_channel_config make_pio_txctrl(PIO pio, uint sm, dma_channel_config ctrl) {
        channel_config_set_transfer_data_size(&ctrl, DMA_SIZE_8);
        channel_config_set_write_increment(&ctrl, false);
        uint txdreq = pio_get_dreq(pio, sm, true);
        channel_config_set_dreq(&ctrl, txdreq);
        // Unlike UART and memory channels, PIO channels use a PIO
        // interrupt to signal tx completions so we set the DMA tx
        // configuration to IRQ quiet mode.
        channel_config_set_irq_quiet(&ctrl, true);
        return ctrl;
}

static inline dma_channel_config make_rxctrl(PIO pio, uint sm, dma_channel_config ctrl) {
        channel_config_set_transfer_data_size(&ctrl, DMA_SIZE_8);
        channel_config_set_read_increment(&ctrl, false);
        uint rxdreq = pio_get_dreq(pio, sm, false);
        channel_config_set_dreq(&ctrl, rxdreq);
        return ctrl;
}

static uint choose_and_claim_sm(PIO pio, int sm_opt) {
        if (sm_opt == -1)
                return (uint)pio_claim_unused_sm(pio, true);

        uint sm = (uint)sm_opt;
        pio_sm_claim(pio, sm);
        return sm;
}

static void init_tx(dmachan_tx_channel_t *tx, pch_pio_config_t *cfg, pch_piochan_config_t *pc) {
        PIO pio = cfg->pio;
        uint sm = choose_and_claim_sm(pio, pc->tx_sm);
        uint32_t hwaddr = (uint32_t)&pio->txf[sm];

        dma_channel_config ctrl = make_pio_txctrl(pio, sm, cfg->ctrl);
        dmachan_1way_config_t c = dmachan_1way_config_claim(hwaddr,
                ctrl, cfg->irq_index);
        dmachan_init_tx_channel(tx, &c, &dmachan_pio_tx_channel_ops);

        tx->u.pio.pio = pio;
        tx->u.pio.sm = sm;
        piochan_tx_pio_init(pio, sm, cfg->tx_offset,
                pc->pins.tx_clock_in, pc->pins.tx_data_out);
        // We do not do dmachan_set_link_dma_irq_enabled(&tx->link, true)
        // because we use the PIO interrupt to be notified when tx
        // is complete.
}

static void init_rx(dmachan_rx_channel_t *rx, pch_pio_config_t *cfg, pch_piochan_config_t *pc) {
        PIO pio = cfg->pio;
        uint sm = choose_and_claim_sm(pio, pc->rx_sm);
        // Add 3 bytes to fetch the high-byte of the FIFO entry which
        // is where the PIO SM shifts each incoming data byte
        uint32_t hwaddr = (uint32_t)&cfg->pio->rxf[sm] + 3;

        dma_channel_config ctrl = make_rxctrl(pio, sm, cfg->ctrl);
        dmachan_1way_config_t c = dmachan_1way_config_claim(hwaddr,
                ctrl, cfg->irq_index);
        dmachan_init_rx_channel(rx, &c, &dmachan_pio_rx_channel_ops);
        rx->u.pio.pio = pio;
        rx->u.pio.sm = sm;
        piochan_rx_pio_init(pio, sm, cfg->rx_offset,
                pc->pins.rx_clock_out, pc->pins.rx_data_in);
        dmachan_set_link_dma_irq_enabled(&rx->link, true);
}

void pch_channel_init_piochan(pch_channel_t *ch, uint8_t id, pch_pio_config_t *cfg, pch_piochan_config_t *pc) {
        assert(!pch_channel_is_started(ch));

        trace_piochan_init(ch, id, cfg, pc);
        init_tx(&ch->tx, cfg, pc);
        init_rx(&ch->rx, cfg, pc);
        pch_channel_configure_id(ch, id);
}

void pch_piochan_init(pch_pio_config_t *cfg) {
        if (cfg->tx_offset == -1) {
                cfg->tx_offset = pio_add_program(cfg->pio, &piochan_tx_program);
                assert(cfg->tx_offset >= 0);
        }

        if (cfg->rx_offset == -1) {
                cfg->rx_offset = pio_add_program(cfg->pio, &piochan_rx_program);
                assert(cfg->rx_offset >= 0);
        }
}
