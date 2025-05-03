/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CSS_CSS_CU_H
#define _PCH_CSS_CSS_CU_H

#include <stdint.h>
#include <stdbool.h>
#include "css_internal.h"
#include "txsm/txsm.h"
#include "proto/packet.h"

// ua_dlist_t is the head of a circular double-linked list of schibs
// which all belong to the same CU, linked by the prevua/nextua
// fields of schib.mda. It is the unit_addr_t of the head of the list
// or else -1 if the list is empty.
typedef int16_t ua_dlist_t;

// ua_slist_t is the head and tail of a single-linked list of schibs
// which all belong to the same CU, linked by the nextua field of
// schib.mda. It contains the unit_addr_t of the head and tail of
// the list or else both fields are -1 if the list is empty.
typedef struct ua_slist {
        int16_t head;
        int16_t tail;
} ua_slist_t;

typedef struct __aligned(4) css_cu {
        dmachan_tx_channel_t    tx_channel;
        dmachan_rx_channel_t    rx_channel;
        pch_txsm_t              tx_pending;
        pch_cunum_t             cunum;
        pch_sid_t               first_sid;
        uint16_t                num_devices; // [0, 256]
        // rx_data_for_ua: rx dma is active writing to CCW for this ua
        int16_t                 rx_data_for_ua;
        // rx_data_end_ds: if non-zero then, when rx data complete,
        // treat as an immediate implicit device status for update_status
        uint8_t                 rx_data_end_ds;
        // rx_response_required: when rx data complete, peer wants response
        bool                    rx_response_required;
        bool                    traced;
        bool                    claimed;
        bool                    configured;
        bool                    started;
        // tx_active: tx dma is active
        bool                    tx_active;
        // ua_func_dlist: links via schib.prevua and .nextua
        ua_dlist_t              ua_func_dlist;
        // ua_response_slist: link via schib.nextua
        ua_slist_t              ua_response_slist;
} css_cu_t;

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

void push_ua_dlist_unsafe(ua_dlist_t *l, css_cu_t *cu, pch_schib_t *schib);

static inline void push_ua_dlist(ua_dlist_t *l, css_cu_t *cu, pch_schib_t *schib) {
        uint32_t status = schibs_lock();
        push_ua_dlist_unsafe(l, cu, schib);
        schibs_unlock(status);
}

pch_schib_t *remove_from_ua_dlist_unsafe(ua_dlist_t *l, css_cu_t *cu, pch_unit_addr_t ua);

static inline pch_schib_t *remove_from_ua_dlist(ua_dlist_t *l, css_cu_t *cu, pch_unit_addr_t ua) {
        uint32_t status = schibs_lock();
        pch_schib_t *schib = remove_from_ua_dlist_unsafe(l, cu, ua);
        schibs_unlock(status);
        return schib;
}

static inline pch_schib_t *pop_ua_dlist_unsafe(ua_dlist_t *l, css_cu_t *cu) {
        if (*l == -1)
                return NULL;

        return remove_from_ua_dlist_unsafe(l, cu, (pch_unit_addr_t)*l);
}

static inline pch_schib_t *pop_ua_dlist(ua_dlist_t *l, css_cu_t *cu) {
        uint32_t status = schibs_lock();
        pch_schib_t *schib = pop_ua_dlist_unsafe(l, cu);
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

pch_schib_t *pop_ua_slist_unsafe(ua_slist_t *l, css_cu_t *cu);

static inline pch_schib_t *pop_ua_slist(ua_slist_t *l, css_cu_t *cu) {
        uint32_t status = schibs_lock();
        pch_schib_t *schib = pop_ua_slist_unsafe(l, cu);
        schibs_unlock(status);
        return schib;
}

bool push_ua_slist_unsafe(ua_slist_t *l, css_cu_t *cu, pch_sid_t sid);

static inline bool push_ua_slist(ua_slist_t *l, css_cu_t *cu, pch_sid_t sid) {
        uint32_t status = schibs_lock();
        bool was_empty = push_ua_slist_unsafe(l, cu, sid);
        schibs_unlock(status);
        return was_empty;
}

// popping from and pushing to the CU ua_response_slist of schibs
// with response packets pending to be sent to their CUs
static inline pch_schib_t *pop_ua_response_slist(css_cu_t *cu) {
        return pop_ua_slist(&cu->ua_response_slist, cu);
}

static inline void push_ua_response_slist(css_cu_t *cu, pch_sid_t sid) {
        push_ua_slist(&cu->ua_response_slist, cu, sid);
}

//
// getting packets to/from the channel command buffers
//
static inline proto_packet_t get_rx_packet(css_cu_t *cu) {
        // cu.rx_channel is a dmachan_rx_channel_t which is
        // __aligned(4) and cmdbuf is the first member of rx_channel
        // so is 4-byte aligned. proto_packet_t is 4-bytes and also
        // __aligned(4) (and needing no more than 4-byte alignment)
        // but omitting the __builtin_assume_aligned below causes
        // gcc 14.1.0 to produce error
        // error: cast increases required alignment of target type
        // [-Werror=cast-align]
        proto_packet_t *pp = (proto_packet_t *)
                __builtin_assume_aligned(cu->rx_channel.cmdbuf, 4);
        return *pp;
}

static inline proto_packet_t get_tx_packet(css_cu_t *cu) {
        // cu.tx_channel is a dmachan_tx_channel_t which is the
        // first member of cu which is a css_cu_t which is
        // __aligned(4) and cmdbuf is the first member of tx_channel
        // so is 4-byte aligned. proto_packet_t is 4-bytes and also
        // __aligned(4) (and needing no more than 4-byte alignment)
        // but omitting the __builtin_assume_aligned below causes
        // gcc 14.1.0 to produce error
        // error: cast increases required alignment of target type
        // [-Werror=cast-align]
        proto_packet_t *pp = (proto_packet_t *)
                __builtin_assume_aligned(cu->tx_channel.cmdbuf, 4);
        return *pp;
}

void send_tx_packet(css_cu_t *cu, proto_packet_t p);

#endif
