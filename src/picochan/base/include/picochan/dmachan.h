/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_API_DMACHAN_H
#define _PCH_API_DMACHAN_H

// PICO_CONFIG: PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN, Enable/disable assertions in the pch_dmachan module, type=bool, default=0, group=pch_dmachan
#ifndef PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN
#define PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN 0
#endif

#ifndef PCH_CONFIG_ENABLE_MEMCHAN
#define PCH_CONFIG_ENABLE_MEMCHAN 1
#endif

#ifndef PCH_UARTCHAN_DEFAULT_BAUDRATE
#define PCH_UARTCHAN_DEFAULT_BAUDRATE 115200
#endif

#include "hardware/dma.h"
#include "hardware/structs/dma_debug.h"
#include "hardware/uart.h"
#include "pico/platform/compiler.h"
#include "picochan/dmachan_defs.h"
#include "picochan/ids.h"
#include "picochan/trc.h"

// General Pico SDK-like DMA-related functions that aren't in the SDK
static inline enum dma_channel_transfer_size channel_config_get_transfer_data_size(dma_channel_config config) {
        uint size = (config.ctrl & DMA_CH0_CTRL_TRIG_DATA_SIZE_BITS) >> DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
        return (enum dma_channel_transfer_size)size;
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

static inline bool dma_irqn_get_channel_forced(uint irq_index, uint channel) {
        invalid_params_if(HARDWARE_DMA, irq_index >= NUM_DMA_IRQS);
        check_dma_channel_param(channel);

        return dma_hw->irq_ctrl[irq_index].intf & (1u << channel);
}

static inline void dma_irqn_set_channel_forced(uint irq_index, uint channel, bool forced) {
        invalid_params_if(HARDWARE_DMA, irq_index >= NUM_DMA_IRQS);

        if (forced)
                hw_set_bits(&dma_hw->irq_ctrl[irq_index].intf, 1u << channel);
        else
                hw_clear_bits(&dma_hw->irq_ctrl[irq_index].intf, 1u << channel);
}

static inline uint32_t dma_get_ctrl_value(uint channel) {
        dma_channel_config_t config = dma_get_channel_config(channel);
        return channel_config_get_ctrl_value(&config);
}

typedef struct pch_uartchan_config {
        dma_channel_config      ctrl;
        uint                    baudrate;
        uint                    irq_index;
} pch_uartchan_config_t;

static inline pch_uartchan_config_t pch_uartchan_get_default_config(uart_inst_t *uart) {
        // Argument 0 to dma_channel_get_default_config is ok here
        // (as would be any DMA id) because it only affects the
        // "chain-to" value and that is overridden when the ctrl
        // value is used.
        return ((pch_uartchan_config_t){
                .ctrl = dma_channel_get_default_config(0),
                .baudrate = PCH_UARTCHAN_DEFAULT_BAUDRATE,
                .irq_index = get_core_num()
        });
}

// DMA configuration for one direction (tx or rx) of a dmachan channel
typedef struct dmachan_1way_config {
        uint32_t                addr;
        dma_channel_config      ctrl;
        pch_dmaid_t             dmaid;
        pch_dma_irq_index_t     dmairqix;
} dmachan_1way_config_t;

static inline dmachan_1way_config_t dmachan_1way_config_make(pch_dmaid_t dmaid, uint32_t addr, dma_channel_config ctrl, pch_dma_irq_index_t dmairqix) {
        return ((dmachan_1way_config_t){
                .addr = addr,
                .ctrl = ctrl,
                .dmaid = dmaid,
                .dmairqix = dmairqix
        });
}

static inline dmachan_1way_config_t dmachan_1way_config_claim(uint32_t addr, dma_channel_config ctrl, pch_dma_irq_index_t dmairqix) {
        pch_dmaid_t dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        return dmachan_1way_config_make(dmaid, addr, ctrl, dmairqix);
}

static inline dmachan_1way_config_t dmachan_1way_config_memchan_make(pch_dmaid_t dmaid, pch_dma_irq_index_t dmairqix) {
        dma_channel_config ctrl = dma_channel_get_default_config(dmaid);
        channel_config_set_transfer_data_size(&ctrl, DMA_SIZE_8);
        channel_config_set_read_increment(&ctrl, true);
        channel_config_set_write_increment(&ctrl, true);
        return ((dmachan_1way_config_t){
                .addr = 0,
                .ctrl = ctrl,
                .dmaid = dmaid,
                .dmairqix = dmairqix
        });
}

// DMA configuration for both directions (tx and rx) of a dmachan
// channel
typedef struct dmachan_config {
        dmachan_1way_config_t   tx;
        dmachan_1way_config_t   rx;
} dmachan_config_t;

static inline dmachan_config_t dmachan_config_claim(uint32_t txaddr, dma_channel_config txctrl, uint32_t rxaddr, dma_channel_config rxctrl, pch_dma_irq_index_t dmairqix) {
        return ((dmachan_config_t){
                .tx = dmachan_1way_config_claim(txaddr, txctrl, dmairqix),
                .rx = dmachan_1way_config_claim(rxaddr, rxctrl, dmairqix)
        });
}

static inline dmachan_config_t dmachan_config_memchan_make(pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, pch_dma_irq_index_t dmairqix) {
        return ((dmachan_config_t){
                .tx = dmachan_1way_config_memchan_make(txdmaid, dmairqix),
                .rx = dmachan_1way_config_memchan_make(rxdmaid, dmairqix)
        });
}

typedef union __aligned(4) dmachan_cmd {
        unsigned char   buf[4];
        uint32_t        raw;        
} dmachan_cmd_t;

#define DMACHAN_CMD_SIZE sizeof(dmachan_cmd_t)
static_assert(DMACHAN_CMD_SIZE == 4, "dmachan_cmd_t must be 4 bytes");

static inline dmachan_cmd_t dmachan_make_cmd_from_word(uint32_t rawcmd) {
        return ((dmachan_cmd_t){.raw = rawcmd});
}

static inline void dmachan_cmd_set_zero(dmachan_cmd_t *cmd) {
        cmd->raw = 0;
}

// dmachan_link_t collects the common fields in tx and rx channels
typedef struct __aligned(4) dmachan_link {
        dmachan_cmd_t           cmd;
        pch_trc_bufferset_t     *bs;            // only when tracing
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
        uint16_t                seqnum;
#endif
        pch_dmaid_t             dmaid;
        pch_dma_irq_index_t     dmairqix;
        bool                    complete;
        bool                    resetting;
} dmachan_link_t;

static inline uint16_t dmachan_link_seqnum(dmachan_link_t *l) {
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
        return l->seqnum;
#else
        return 0;
#endif
}

static inline void dmachan_set_link_bs(dmachan_link_t *l, pch_trc_bufferset_t *bs) {
        l->bs = bs;
}

static inline void dmachan_link_cmd_set_zero(dmachan_link_t *l) {
        dmachan_cmd_set_zero(&l->cmd);
}

static inline void dmachan_link_cmd_set(dmachan_link_t *l, dmachan_cmd_t cmd) {
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
        l->seqnum++;
#endif
        l->cmd.raw = cmd.raw;
}

static inline void dmachan_link_cmd_copy(dmachan_link_t *dst, dmachan_link_t *src) {
        dst->cmd.raw = src->cmd.raw;
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
        dst->seqnum = src->seqnum;
#endif
}

static inline void dmachan_set_link_irq_enabled(dmachan_link_t *l, bool enabled) {
        pch_dma_irq_index_t dmairqix = l->dmairqix;
        assert(dmairqix >= 0 && dmairqix < NUM_DMA_IRQS);
        dma_irqn_set_channel_enabled(dmairqix, l->dmaid, enabled);
}

static inline bool dmachan_link_irq_raised(dmachan_link_t *l) {
        return dma_irqn_get_channel_status(l->dmairqix, l->dmaid);
};

static inline bool dmachan_get_link_irq_forced(dmachan_link_t *l) {
        return dma_irqn_get_channel_forced(l->dmairqix, l->dmaid);
}

static inline void dmachan_set_link_irq_forced(dmachan_link_t *l, bool forced) {
        dma_irqn_set_channel_forced(l->dmairqix, l->dmaid, forced);
}

static inline void dmachan_ack_link_irq(dmachan_link_t *l) {
        dma_irqn_acknowledge_channel(l->dmairqix, l->dmaid);
}

// tx and rx channels, starting with forward declarations because
// for memchans there is a field pointing at the peer channel
typedef struct __aligned(4) dmachan_tx_channel dmachan_tx_channel_t;
typedef struct __aligned(4) dmachan_rx_channel dmachan_rx_channel_t;

typedef struct dmachan_tx_channel_ops {
        void (*start_src_cmdbuf)(dmachan_tx_channel_t *tx);
        void (*write_src_reset)(dmachan_tx_channel_t *tx);
        void (*start_src_data)(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count);
        dmachan_irq_state_t (*handle_tx_irq)(dmachan_tx_channel_t *tx);
} dmachan_tx_channel_ops_t;

typedef struct __aligned(4) dmachan_tx_channel {
        dmachan_link_t          link;
        const dmachan_tx_channel_ops_t *ops;
        dmachan_rx_channel_t    *mem_rx_peer;   // only for memchan
        dmachan_mem_src_state_t mem_src_state;  // only for memchan
} dmachan_tx_channel_t;

typedef struct dmachan_rx_channel_ops {
        void (*start_dst_cmdbuf)(dmachan_rx_channel_t *rx);
        void (*start_dst_reset)(dmachan_rx_channel_t *rx);
        void (*start_dst_data)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
        void (*start_dst_discard)(dmachan_rx_channel_t *rx, uint32_t count);
        void (*prep_dst_data_src_zeroes)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
        dmachan_irq_state_t (*handle_rx_irq)(dmachan_rx_channel_t *rx);
} dmachan_rx_channel_ops_t;

typedef struct __aligned(4) dmachan_rx_channel {
        dmachan_link_t          link;
        const dmachan_rx_channel_ops_t *ops;
        dmachan_tx_channel_t    *mem_tx_peer;   // only for memchan
        uint32_t                srcaddr;
        dma_channel_config      ctrl;
        dmachan_mem_dst_state_t mem_dst_state;  // only for memchan
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
        uint16_t                seen_seqnum;
#endif
} dmachan_rx_channel_t;

typedef struct pch_channel {
        dmachan_tx_channel_t    tx;
        dmachan_rx_channel_t    rx;
} pch_channel_t;

static inline dmachan_irq_state_t dmachan_make_irq_state(bool raised, bool forced, bool complete) {
        return ((dmachan_irq_state_t)raised)
                | ((dmachan_irq_state_t)forced) << 1
                | ((dmachan_irq_state_t)complete) << 2;
}

// tx channel irq and memory source state handling
static inline void dmachan_set_mem_src_state(dmachan_tx_channel_t *tx, dmachan_mem_src_state_t new_state) {
        valid_params_if(PCH_DMACHAN,
                new_state == DMACHAN_MEM_SRC_IDLE
                || tx->mem_src_state == DMACHAN_MEM_SRC_IDLE);

        tx->mem_src_state = new_state;
}

// rx channel irq and memory destination state handling
static inline void dmachan_set_mem_dst_state(dmachan_rx_channel_t *rx, dmachan_mem_dst_state_t new_state) {
        valid_params_if(PCH_DMACHAN,
                new_state == DMACHAN_MEM_DST_IDLE
                || rx->mem_dst_state == DMACHAN_MEM_DST_IDLE);

        rx->mem_dst_state = new_state;
}

void dmachan_panic_unless_memchan_initialised(void);

// Methods for dmachan_rx_channel_t

static inline void dmachan_start_src_cmdbuf(dmachan_tx_channel_t *tx) {
        tx->ops->start_src_cmdbuf(tx);
}

static inline void dmachan_write_src_reset(dmachan_tx_channel_t *tx) {
        tx->ops->write_src_reset(tx);
}

static inline void dmachan_start_src_data(dmachan_tx_channel_t *tx, uint32_t srcaddr, uint32_t count) {
        tx->ops->start_src_data(tx, srcaddr, count);
}

static inline dmachan_irq_state_t dmachan_handle_tx_irq(dmachan_tx_channel_t *tx) {
        return tx->ops->handle_tx_irq(tx);
}

// Methods for dmachan_rx_channel_t

static inline void dmachan_start_dst_cmdbuf(dmachan_rx_channel_t *rx) {
        rx->ops->start_dst_cmdbuf(rx);
}

static inline void dmachan_start_dst_reset(dmachan_rx_channel_t *rx) {
        rx->ops->start_dst_reset(rx);
}

static inline void dmachan_start_dst_data(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count) {
        rx->ops->start_dst_data(rx, dstaddr, count);
}

static inline void dmachan_start_dst_discard(dmachan_rx_channel_t *rx, uint32_t count) {
        rx->ops->start_dst_discard(rx, count);
}

static inline dmachan_irq_state_t dmachan_handle_rx_irq(dmachan_rx_channel_t *rx) {
        return rx->ops->handle_rx_irq(rx);
}

void dmachan_start_dst_data_src_zeroes(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);


extern dmachan_rx_channel_ops_t dmachan_mem_rx_channel_ops;
extern dmachan_tx_channel_ops_t dmachan_mem_tx_channel_ops;
extern dmachan_rx_channel_ops_t dmachan_uart_rx_channel_ops;
extern dmachan_tx_channel_ops_t dmachan_uart_tx_channel_ops;

// Convenience functions for configuring UART channels
void pch_uart_init(uart_inst_t *uart, uint baudrate);

void dmachan_init_uart_channel(pch_channel_t *ch, uart_inst_t *uart, pch_uartchan_config_t *cfg);
void dmachan_init_mem_channel(pch_channel_t *ch, dmachan_config_t *dc, pch_channel_t *chpeer);

// pch_memchan_init must be called before configuring either side of
// any memchan CU with pch_cus_memcu_configure or
// pch_chp_configure_memchan
void pch_memchan_init(void);

#endif
