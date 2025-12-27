/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CSS_CHANNEL_H
#define _PCH_CSS_CHANNEL_H

#include <stdint.h>
#include <stdbool.h>
#include "css_internal.h"
#include "txsm/txsm.h"
#include "proto/packet.h"

/*! \file css/channel.h
 *  \defgroup internal_css internal_css
 *
 * \brief A (CSS-side) channel that connects to a remote CU
 */

// ua_dlist_t is the head of a circular double-linked list of schibs
// which all belong to the same channel, linked by the prevua/nextua
// fields of schib.mda. It is the unit_addr_t of the head of the list
// or else -1 if the list is empty.
typedef int16_t ua_dlist_t;

// ua_slist_t is the head and tail of a single-linked list of schibs
// which all belong to the same channel, linked by the nextua field of
// schib.mda. It contains the unit_addr_t of the head and tail of
// the list or else both fields are -1 if the list is empty.
typedef struct ua_slist {
        int16_t head;
        int16_t tail;
} ua_slist_t;

/*! \brief pch_chp_t is the CSS-side representation of a channel path
 * to a control unit.
 * \ingroup internal_css
 *
 * The application API usually refers to these by a channel path id
 * (CHPID) which indexes into the global array CSS.chps and so does
 * not really need to care about the details of this struct.
 * Currently, a channel only connects to a single control unit so
 * the pch_chp_t is effectively a CSS-side "peer" object of the
 * dev-side CU, pch_cu_t.
 */
typedef struct __aligned(4) pch_chp {
        pch_channel_t           channel;
        pch_txsm_t              tx_pending;
        pch_sid_t               first_sid;
        uint16_t                num_devices; // [0, 256]
        // rx_data_for_ua: rx dma is active writing to CCW for this ua
        int16_t                 rx_data_for_ua;
        // rx_data_end_ds: if non-zero then, when rx data complete,
        // treat as an immediate implicit device status for update_status
        uint8_t                 rx_data_end_ds;
        uint8_t                 flags;
        uint8_t                 trace_flags;
        // ua_func_dlist: links via schib.prevua and .nextua
        ua_dlist_t              ua_func_dlist;
        // ua_response_slist: link via schib.nextua
        ua_slist_t              ua_response_slist;
} pch_chp_t;

// values for pch_chp_t flags
// rx_response_required: when rx data complete, peer wants response
#define PCH_CHP_RX_RESPONSE_REQUIRED    0x01
#define PCH_CHP_CLAIMED                 0x02
#define PCH_CHP_ALLOCATED               0x04
#define PCH_CHP_CONFIGURED              0x08
#define PCH_CHP_STARTED                 0x10
// tx_active: tx dma is active
#define PCH_CHP_TX_ACTIVE               0x20

static inline bool pch_chp_is_rx_response_required(pch_chp_t *chp) {
        return chp->flags & PCH_CHP_RX_RESPONSE_REQUIRED;
}

static inline bool pch_chp_is_claimed(pch_chp_t *chp) {
        return chp->flags & PCH_CHP_CLAIMED;
}

static inline bool pch_chp_is_allocated(pch_chp_t *chp) {
        return chp->flags & PCH_CHP_ALLOCATED;
}

static inline bool pch_chp_is_configured(pch_chp_t *chp) {
        return chp->flags & PCH_CHP_CONFIGURED;
}

static inline bool pch_chp_is_started(pch_chp_t *chp) {
        return chp->flags & PCH_CHP_STARTED;
}

static inline bool pch_chp_is_tx_active(pch_chp_t *chp) {
        return chp->flags & PCH_CHP_TX_ACTIVE;
}

static inline void pch_chp_set_rx_response_required(pch_chp_t *chp, bool b) {
        if (b)
                chp->flags |= PCH_CHP_RX_RESPONSE_REQUIRED;
        else
                chp->flags &= ~PCH_CHP_RX_RESPONSE_REQUIRED;
}

static inline void pch_chp_set_claimed(pch_chp_t *chp, bool b) {
        if (b)
                chp->flags |= PCH_CHP_CLAIMED;
        else
                chp->flags &= ~PCH_CHP_CLAIMED;
}

static inline void pch_chp_set_allocated(pch_chp_t *chp, bool b) {
        if (b)
                chp->flags |= PCH_CHP_ALLOCATED;
        else
                chp->flags &= ~PCH_CHP_ALLOCATED;
}

static inline void pch_chp_set_configured(pch_chp_t *chp, bool b) {
        if (b)
                chp->flags |= PCH_CHP_CONFIGURED;
        else
                chp->flags &= ~PCH_CHP_CONFIGURED;
}

static inline void pch_chp_set_started(pch_chp_t *chp, bool b) {
        if (b)
                chp->flags |= PCH_CHP_STARTED;
        else
                chp->flags &= ~PCH_CHP_STARTED;
}

static inline void pch_chp_set_tx_active(pch_chp_t *chp, bool b) {
        if (b)
                chp->flags |= PCH_CHP_TX_ACTIVE;
        else
                chp->flags &= ~PCH_CHP_TX_ACTIVE;
}

static inline bool pch_chp_is_traced_general(pch_chp_t *chp) {
        return chp->trace_flags & PCH_CHP_TRACED_GENERAL;
}

static inline bool pch_chp_is_traced_link(pch_chp_t *chp) {
        return chp->trace_flags & PCH_CHP_TRACED_LINK;
}

static inline bool pch_chp_is_traced_irq(pch_chp_t *chp) {
        return chp->trace_flags & PCH_CHP_TRACED_IRQ;
}

//
// ua_dlist
//
#define EMPTY_UA_DLIST ((ua_dlist_t)-1)

static inline ua_dlist_t make_ua_dlist(void) {
        return EMPTY_UA_DLIST;
}

static inline int16_t peek_ua_dlist(ua_dlist_t *l) {
        return (int16_t)*l;
}

void push_ua_dlist_unsafe(ua_dlist_t *l, pch_chp_t *chp, pch_schib_t *schib);

static inline void push_ua_dlist(ua_dlist_t *l, pch_chp_t *chp, pch_schib_t *schib) {
        uint32_t status = schibs_lock();
        push_ua_dlist_unsafe(l, chp, schib);
        schibs_unlock(status);
}

pch_schib_t *remove_from_ua_dlist_unsafe(ua_dlist_t *l, pch_chp_t *chp, pch_unit_addr_t ua);

static inline pch_schib_t *remove_from_ua_dlist(ua_dlist_t *l, pch_chp_t *chp, pch_unit_addr_t ua) {
        uint32_t status = schibs_lock();
        pch_schib_t *schib = remove_from_ua_dlist_unsafe(l, chp, ua);
        schibs_unlock(status);
        return schib;
}

static inline pch_schib_t *pop_ua_dlist_unsafe(ua_dlist_t *l, pch_chp_t *chp) {
        if (*l == -1)
                return NULL;

        return remove_from_ua_dlist_unsafe(l, chp, (pch_unit_addr_t)*l);
}

static inline pch_schib_t *pop_ua_dlist(ua_dlist_t *l, pch_chp_t *chp) {
        uint32_t status = schibs_lock();
        pch_schib_t *schib = pop_ua_dlist_unsafe(l, chp);
        schibs_unlock(status);
        return schib;
}

//
// ua_slist
//

static inline ua_slist_t make_ua_slist(void) {
        return ((ua_slist_t){-1, -1});
}

static inline void reset_ua_slist(ua_slist_t *l) {
        l->head = -1;
        l->tail = -1;
}

pch_schib_t *pop_ua_slist_unsafe(ua_slist_t *l, pch_chp_t *chp);

static inline pch_schib_t *pop_ua_slist(ua_slist_t *l, pch_chp_t *chp) {
        uint32_t status = schibs_lock();
        pch_schib_t *schib = pop_ua_slist_unsafe(l, chp);
        schibs_unlock(status);
        return schib;
}

bool push_ua_slist_unsafe(ua_slist_t *l, pch_chp_t *chp, pch_sid_t sid);

static inline bool push_ua_slist(ua_slist_t *l, pch_chp_t *chp, pch_sid_t sid) {
        uint32_t status = schibs_lock();
        bool was_empty = push_ua_slist_unsafe(l, chp, sid);
        schibs_unlock(status);
        return was_empty;
}

// popping from and pushing to the channel ua_response_slist of schibs
// with response packets pending to be sent to their CUs
static inline pch_schib_t *pop_ua_response_slist(pch_chp_t *chp) {
        return pop_ua_slist(&chp->ua_response_slist, chp);
}

static inline void push_ua_response_slist(pch_chp_t *chp, pch_sid_t sid) {
        push_ua_slist(&chp->ua_response_slist, chp, sid);
}

//
// getting packets to/from the channel command buffers
//

static inline proto_packet_t get_tx_packet(pch_chp_t *chp) {
        // chp.tx_channel is a dmachan_tx_channel_t which is the
        // first member of chp which is a pch_chp_t which is
        // __aligned(4) and cmd is the first member of tx_channel
        // so is 4-byte aligned. proto_packet_t is 4-bytes and also
        // __aligned(4) (and needing no more than 4-byte alignment)
        // but omitting the __builtin_assume_aligned below causes
        // gcc 14.1.0 to produce error
        // error: cast increases required alignment of target type
        // [-Werror=cast-align]
        proto_packet_t *pp = (proto_packet_t *)
                __builtin_assume_aligned(&chp->channel.tx.link.cmd, 4);
        return *pp;
}

void send_tx_packet(pch_chp_t *chp, pch_schib_t *schib, proto_packet_t p);

#endif
