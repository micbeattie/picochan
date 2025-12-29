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

static inline dma_debug_channel_hw_t *dma_debug_channel_hw_addr(uint channel) {
        check_dma_channel_param(channel);
        return &dma_debug_hw->ch[channel];
}

static inline uint32_t dma_channel_get_reload_count(uint channel) {
        return dma_debug_channel_hw_addr(channel)->dbg_tcr;
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
        pch_trc_bufferset_t     *bs;    // set/unset by owning channel
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
        uint16_t                seqnum;
#endif
        pch_dmaid_t             dmaid;
        pch_irq_index_t         irq_index;
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

typedef struct dmachan_mem_tx_channel_data {
        dmachan_rx_channel_t    *rx_peer;
        dmachan_mem_src_state_t src_state;
} dmachan_mem_tx_channel_data_t;

typedef union {
        dmachan_mem_tx_channel_data_t   mem;
} dmachan_tx_channel_data_t;

typedef struct __aligned(4) dmachan_tx_channel {
        dmachan_link_t                  link;
        const dmachan_tx_channel_ops_t  *ops;
        dmachan_tx_channel_data_t       u;
} dmachan_tx_channel_t;

typedef struct dmachan_rx_channel_ops {
        void (*start_dst_cmdbuf)(dmachan_rx_channel_t *rx);
        void (*start_dst_reset)(dmachan_rx_channel_t *rx);
        void (*start_dst_data)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
        void (*start_dst_discard)(dmachan_rx_channel_t *rx, uint32_t count);
        void (*prep_dst_data_src_zeroes)(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);
        dmachan_irq_state_t (*handle_rx_irq)(dmachan_rx_channel_t *rx);
} dmachan_rx_channel_ops_t;

typedef struct dmachan_mem_rx_channel_data {
        dmachan_tx_channel_t    *tx_peer;
        dmachan_mem_dst_state_t dst_state;
} dmachan_mem_rx_channel_data_t;

typedef union {
        dmachan_mem_rx_channel_data_t   mem;
} dmachan_rx_channel_data_t;

typedef struct __aligned(4) dmachan_rx_channel {
        dmachan_link_t                  link;
        const dmachan_rx_channel_ops_t  *ops;
        uint32_t                        srcaddr;
        dma_channel_config              ctrl;
#ifdef PCH_CONFIG_DEBUG_MEMCHAN
        uint16_t                        seen_seqnum;
#endif
        dmachan_rx_channel_data_t       u;
} dmachan_rx_channel_t;

typedef struct pch_channel {
        dmachan_tx_channel_t    tx;
        dmachan_rx_channel_t    rx;
        uint8_t                 flags;
        uint8_t                 id;
} pch_channel_t;

// Values of pch_channel_t flags field
#define PCH_CHANNEL_CONFIGURED  0x01
#define PCH_CHANNEL_STARTED     0x02
#define PCH_CHANNEL_TRACED      0x04

static inline bool pch_channel_is_configured(pch_channel_t *ch) {
        return ch->flags & PCH_CHANNEL_CONFIGURED;
}

static inline bool pch_channel_is_started(pch_channel_t *ch) {
        return ch->flags & PCH_CHANNEL_STARTED;
}

static inline bool pch_channel_is_traced(pch_channel_t *ch) {
        return ch->flags & PCH_CHANNEL_TRACED;
}

static inline void pch_channel_configure_id(pch_channel_t *ch, uint8_t id) {
        assert(!pch_channel_is_configured(ch));
        ch->id = id;
        ch->flags |= PCH_CHANNEL_CONFIGURED;
}

static inline void pch_channel_set_unconfigured(pch_channel_t *ch) {
        ch->flags &= ~PCH_CHANNEL_CONFIGURED;
        ch->id = 0;
}

static inline void pch_channel_set_started(pch_channel_t *ch, bool b) {
        if (b)
                ch->flags |= PCH_CHANNEL_STARTED;
        else
                ch->flags &= ~PCH_CHANNEL_STARTED;
}

static inline void pch_channel_trace(pch_channel_t *ch, pch_trc_bufferset_t *bs) {
        if (bs) {
                ch->tx.link.bs = bs;
                ch->rx.link.bs = bs;
                ch->flags |= PCH_CHANNEL_TRACED;
        } else {
                ch->tx.link.bs = NULL;
                ch->rx.link.bs = NULL;
                ch->flags &= ~PCH_CHANNEL_TRACED;
        }
}

// tx channel irq and memory source state handling
static inline void dmachan_set_mem_src_state(dmachan_tx_channel_t *tx, dmachan_mem_src_state_t new_state) {
        valid_params_if(PCH_DMACHAN,
                new_state == DMACHAN_MEM_SRC_IDLE
                || tx->u.mem.src_state == DMACHAN_MEM_SRC_IDLE);

        tx->u.mem.src_state = new_state;
}

// rx channel irq and memory destination state handling
static inline void dmachan_set_mem_dst_state(dmachan_rx_channel_t *rx, dmachan_mem_dst_state_t new_state) {
        valid_params_if(PCH_DMACHAN,
                new_state == DMACHAN_MEM_DST_IDLE
                || rx->u.mem.dst_state == DMACHAN_MEM_DST_IDLE);

        rx->u.mem.dst_state = new_state;
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

void dmachan_start_dst_data_src_zeroes(dmachan_rx_channel_t *rx, uint32_t dstaddr, uint32_t count);

// Convenience functions for configuring UART channels
void pch_uart_init(uart_inst_t *uart, uint baudrate);

void pch_channel_init_uartchan(pch_channel_t *ch, uint8_t id, uart_inst_t *uart, pch_uartchan_config_t *cfg);
void pch_channel_init_memchan(pch_channel_t *ch, uint8_t id, uint dmairqix, pch_channel_t *chpeer);

// pch_memchan_init must be called before configuring either side of
// any memchan CU with pch_cus_memcu_configure or
// pch_chp_configure_memchan
void pch_memchan_init(void);

// pch_channel_handle_dma_irq() must be called for each channel
// whenever there is a DMA interrupt that may be relevant to it.
void pch_channel_handle_dma_irq(pch_channel_t *ch);

#endif
