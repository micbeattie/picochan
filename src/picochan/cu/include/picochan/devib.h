/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_DEVIB_H
#define _PCH_API_DEVIB_H

#include "pico/platform/compiler.h"
#include "picochan/ids.h"
#include "proto/chop.h"
#include "proto/payload.h"

// pch_cbindex_t is an 8-bit index into pch_devib_callbacks, an array
// of up to NUM_DEVIB_CALLBACKS registered callbacks on devibs.
typedef uint8_t pch_cbindex_t;

#define PCH_DEVIB_CALLBACK_DEFAULT      0
#define PCH_DEVIB_CALLBACK_NOOP         255

// MAX_DEVIB_CALLBACKS is the maximum number of registered callbacks.
// A callback index greater than this is handled internally.
#define MAX_DEVIB_CALLBACKS 254

#define NUM_DEVIB_CALLBACKS 16
static_assert(NUM_DEVIB_CALLBACKS <= MAX_DEVIB_CALLBACKS,
        "NUM_DEVIB_CALLBACKS must not exceed MAX_DEVIB_CALLBACKS");

typedef union pch_devdata {
        uint32_t u32;
} pch_devdata_t;
static_assert(sizeof(pch_devdata_t) == 4,
        "pch_devdata_t must be 4 bytes");

// DEVIB  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |     next      |    cbindex    |          size                 |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |       op      |     flags     |         payload               |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                          bufaddr                              |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                          devdata                              |
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
typedef struct __aligned(4) pch_devib {
        pch_unit_addr_t next;
        pch_cbindex_t   cbindex;
        uint16_t        size;
        proto_chop_t    op;
        uint8_t         flags;
        proto_payload_t payload;
        uint32_t        addr;
        pch_devdata_t   devdata;
} pch_devib_t;
static_assert(sizeof(pch_devib_t) == 16,
        "pch_devib_t must be 16 bytes");

#define PCH_DEVIB_FLAG_STARTED          0x80
#define PCH_DEVIB_FLAG_CMD_WRITE        0x40
#define PCH_DEVIB_FLAG_RX_DATA_REQUIRED 0x20
#define PCH_DEVIB_FLAG_TX_CALLBACK      0x10
#define PCH_DEVIB_FLAG_TRACED           0x08

static inline bool pch_devib_is_started(pch_devib_t *devib) {
        return devib->flags & PCH_DEVIB_FLAG_STARTED;
}

static inline bool pch_devib_is_cmd_write(pch_devib_t *devib) {
        return devib->flags & PCH_DEVIB_FLAG_CMD_WRITE;
}

static inline bool pch_devib_is_traced(pch_devib_t *devib) {
        return devib->flags & PCH_DEVIB_FLAG_TRACED;
}

static inline bool pch_devib_set_traced(pch_devib_t *devib, bool trace) {
        bool old_trace = pch_devib_is_traced(devib);
        if (trace)
                devib->flags |= PCH_DEVIB_FLAG_TRACED;
        else
                devib->flags &= ~PCH_DEVIB_FLAG_TRACED;
        
        return old_trace;
}

// Forward declaration of pch_cu_t for identifying devib by
// (pch_cu_t, pch_unit_addr_t) for callbacks and dev implementations.
typedef struct pch_cu pch_cu_t;

// Callbacks


typedef void (*pch_devib_callback_t)(pch_cu_t *cu, pch_devib_t *devib);

extern pch_devib_callback_t pch_devib_callbacks[];

static inline bool pch_cbindex_is_callable(uint cbindex) {
        if (cbindex == PCH_DEVIB_CALLBACK_NOOP)
                return true;

        if (cbindex >= NUM_DEVIB_CALLBACKS)
                return false;

        return pch_devib_callbacks[cbindex] != NULL;
}

// Callback registration API
void pch_register_devib_callback(pch_cbindex_t n, pch_devib_callback_t cb);
pch_cbindex_t pch_register_unused_devib_callback(pch_devib_callback_t cb);

void pch_default_devib_callback(pch_cu_t *cu, pch_devib_t *devib);

// Low-level API for dev implementation updating devib
static inline void pch_devib_prepare_callback(pch_devib_t *devib, pch_cbindex_t cbindex) {
        assert(pch_cbindex_is_callable(cbindex));
        devib->cbindex = cbindex;
}

static inline void pch_devib_prepare_count(pch_devib_t *devib, uint16_t count) {
        devib->payload = proto_make_count_payload(count);
}

static inline void pch_devib_prepare_write_data(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags) {
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        pch_devib_prepare_count(devib, n);
        devib->op = PROTO_CHOP_DATA | flags;
        devib->addr = (uint32_t)srcaddr;
}

static inline void pch_devib_prepare_write_zeroes(pch_devib_t *devib, uint16_t n) {
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        pch_devib_prepare_count(devib, n);
        // hard-code the ResponseRequired flag for now
        devib->op = PROTO_CHOP_DATA | PROTO_CHOP_FLAG_SKIP
                | PROTO_CHOP_FLAG_RESPONSE_REQUIRED;
}

static inline void pch_devib_prepare_read_data(pch_devib_t *devib, void *dstaddr, uint16_t size) {
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        pch_devib_prepare_count(devib, size);
        devib->op = PROTO_CHOP_REQUEST_READ;
        devib->flags |= PCH_DEVIB_FLAG_RX_DATA_REQUIRED;
        devib->addr = (uint32_t)dstaddr;
}

void pch_devib_prepare_update_status(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size);

void pch_devib_send_or_queue_command(pch_cu_t *cu, pch_unit_addr_t ua);

// Slightly higher-level API for dev implementation.
// They return negative error values on error (e.g. -EINVAL).
// They do various parameter checks and return errors instead of
// asserting like the low-level API does. Those with cbindex_opt
// arguments leave the devib cbindex field alone if called with a
// negative value, otherwise they validate it as a callback cbindex
// and set the field or return a negative error value, as appropriate.
// For sends (of data or zeroes), the length sent is validated to be
// under the CSS-advertised window (devib->size) and an error is
// returned if not.

enum {
        ENOSUCHERROR	        = 1,
        EINVALIDCALLBACK	= 2,
        ENOTSTARTED		= 3,
        ECMDNOTREAD		= 4,
        ECMDNOTWRITE		= 5,
        EWRITETOOBIG		= 6,
        EINVALIDSTATUS		= 7,
        EINVALIDDEV             = 8,
        EINVALIDCMD             = 9,
};

int pch_dev_set_callback(pch_cu_t *cu, pch_unit_addr_t ua, int cbindex_opt);
int pch_dev_send(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n);
int pch_dev_send_then(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n);
int pch_dev_send_zeroes_then(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, int cbindex_opt);
int pch_dev_receive(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size);
int pch_dev_receive_then(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size, int cbindex_opt);
int pch_dev_update_status(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs);
int pch_dev_update_status_then(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, int cbindex_opt);
int pch_dev_update_status_advert(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, void *dstaddr, uint16_t size);
int pch_dev_update_status_advert_then(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, void *dstaddr, uint16_t size, int cbindex_opt);

#endif
