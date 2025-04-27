/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_DMACHAN_H
#define _PCH_API_DMACHAN_H

// PICO_CONFIG: PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN, Enable/disable assertions in the pch_dmachan module, type=bool, default=0, group=pch_dmachan
#ifndef PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN
#define PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN 0
#endif

#include "hardware/dma.h"
#include "hardware/structs/dma_debug.h"
#include "hardware/uart.h"
#include "pico/platform/compiler.h"
#include "picochan/ids.h"

#define CMDBUF_SIZE 4

enum __packed dmachan_mem_src_state {
        DMACHAN_MEM_SRC_IDLE = 0,
        DMACHAN_MEM_SRC_CMDBUF,
        DMACHAN_MEM_SRC_DATA
};

enum __packed dmachan_mem_dst_state {
        DMACHAN_MEM_DST_IDLE = 0,
        DMACHAN_MEM_DST_CMDBUF,
        DMACHAN_MEM_DST_DATA,
        DMACHAN_MEM_DST_DISCARD,
        DMACHAN_MEM_DST_SRC_ZEROES
};

typedef struct dmachan_1way_config {
        pch_dmaid_t             dmaid;
        uint32_t                addr;
        dma_channel_config      ctrl;
} dmachan_1way_config_t;

typedef struct dmachan_config {
        dmachan_1way_config_t   tx;
        dmachan_1way_config_t   rx;
} dmachan_config_t;

dmachan_1way_config_t dmachan_1way_config_claim(uint32_t addr, dma_channel_config ctrl);
dmachan_config_t dmachan_config_claim(uint32_t txaddr, dma_channel_config txctrl, uint32_t rxaddr, dma_channel_config rxctrl);

static inline dmachan_1way_config_t dmachan_1way_config_make(pch_dmaid_t dmaid, uint32_t addr, dma_channel_config ctrl) {
        return ((dmachan_1way_config_t){dmaid, addr, ctrl});
}

typedef struct __aligned(4) dmachan_tx_channel dmachan_tx_channel_t;
typedef struct __aligned(4) dmachan_rx_channel dmachan_rx_channel_t;

typedef struct __aligned(4) dmachan_tx_channel {
        unsigned char cmdbuf[4]; // struct __aligned(4) ensures 4-byte aligned
        dmachan_rx_channel_t *mem_rx_peer; // only set for a core-to-core memory channel
        pch_dmaid_t dmaid;
        enum dmachan_mem_src_state mem_src_state; // only used if mem_rx_peer set
} dmachan_tx_channel_t;

typedef struct __aligned(4) dmachan_rx_channel {
        unsigned char cmdbuf[4]; // struct __aligned(4) ensures 4-byte aligned
        dmachan_tx_channel_t *mem_tx_peer; // only set for a core-to-core memory channel
        uint32_t srcaddr;
        dma_channel_config ctrl;
        pch_dmaid_t dmaid;
        enum dmachan_mem_dst_state mem_dst_state; // only used if mem_rx_peer set
} dmachan_rx_channel_t;

static inline enum dma_channel_transfer_size channel_config_get_transfer_data_size(dma_channel_config config) {
        uint size = (config.ctrl & DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS) >> DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
        return (enum dma_channel_transfer_size)size;
}

static inline bool channel_config_get_incr_write(dma_channel_config config) {
        return (config.ctrl & DMA_CH0_CTRL_TRIG_INCR_WRITE_BITS) != 0;
}

static inline bool channel_config_get_incr_read(dma_channel_config config) {
        return (config.ctrl & DMA_CH0_CTRL_TRIG_INCR_READ_BITS) != 0;
}

static inline uint32_t dma_channel_get_transfer_count(uint channel) {
        return dma_channel_hw_addr(channel)->transfer_count;
}

static inline dma_debug_channel_hw_t *dma_debug_channel_hw_addr(uint channel) {
        check_dma_channel_param(channel);
        return &dma_debug_hw->ch[channel];
}

static inline uint32_t dma_channel_get_reload_count(uint channel) {
        return dma_debug_channel_hw_addr(channel)->dbg_tcr;
}

// trigger_irq sets the control register for dmaid with just the
// minimal bits (Enabled and Quiet) and then writes a 0 to the
// register so that it raises the IRQ from this channel without
// actual doing the DMA copy. The write of zero leaves the
// control register as zero so we rely on the next use of the
// DMA writing the whole control register correctly again.
static inline void trigger_irq(pch_dmaid_t dmaid) {
        dma_channel_config czero = {0}; // zero, *not* default config
        dma_channel_config c = czero;
        channel_config_set_irq_quiet(&c, true);
        channel_config_set_enable(&c, true);
        dma_channel_set_config(dmaid, &c, false);
        dma_channel_set_config(dmaid, &czero, true);
}

static inline bool dmachan_tx_irq_raised(dmachan_tx_channel_t *tx, pch_dma_irq_index_t dmairqix) {
        return dma_irqn_get_channel_status(dmairqix, tx->dmaid);
};

static inline void dmachan_ack_tx_irq(dmachan_tx_channel_t *tx, pch_dma_irq_index_t dmairqix) {
        dma_irqn_acknowledge_channel(dmairqix, tx->dmaid);
}

static inline bool dmachan_rx_irq_raised(dmachan_rx_channel_t *rx, pch_dma_irq_index_t dmairqix) {
        return dma_irqn_get_channel_status(dmairqix, rx->dmaid);
};

static inline void dmachan_ack_rx_irq(dmachan_rx_channel_t *rx, pch_dma_irq_index_t dmairqix) {
        dma_irqn_acknowledge_channel(dmairqix, rx->dmaid);
}

void dmachan_init_tx_channel(dmachan_tx_channel_t *tx, dmachan_1way_config_t *cfg);
void dmachan_start_src_cmdbuf(dmachan_tx_channel_t *tx);
void dmachan_start_src_data(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count);

void dmachan_init_rx_channel(dmachan_rx_channel_t *rx, dmachan_1way_config_t *cfg);
void dmachan_start_dst_cmdbuf(dmachan_rx_channel_t *rx);
void dmachan_start_dst_data(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
void dmachan_start_dst_discard(dmachan_rx_channel_t *rx, uint32_t count);
void dmachan_start_dst_data_src_zeroes(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);

// Convenience method for initialising a UART with settings that
// work for both the CSS and CU side of a uart-connected channel
void pch_init_uart(uart_inst_t *uart);

#endif
