/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_PROTO_PAYLOAD_H
#define _PCH_PROTO_PAYLOAD_H

#include "picochan/bsize.h"

// proto_payload_t is a 2-byte channel operation payload. It can be a
// count, a pair of bytes "ccwcmd", "esize" for START-like or a
// byte of device status followed by an (optional) advertised write
// window esize for a device status update operation. A payload of a
// uint16_t is decoded as big endian.
typedef struct proto_payload {
        uint8_t p0;
        uint8_t p1;
} proto_payload_t;

// proto_parse_count_payload parses the payload as a 2-byte
// big-endian value
static inline uint16_t proto_parse_count_payload(proto_payload_t p) {
        return ((uint16_t)p.p0 << 8) + (uint16_t)p.p1; // big endian
}

static inline uint8_t proto_parse_devstatus_payload_devs(proto_payload_t p) {
        return p.p0;
}

static inline pch_bsize_t proto_parse_devstatus_payload_esize(proto_payload_t p) {
        return pch_bsize_wrap(p.p1);
}

struct proto_parsed_devstatus_payload {
        uint16_t count;
        uint8_t devs;
};

static inline proto_payload_t proto_make_count_payload(uint16_t count) {
        // big-endian encoding
        return ((proto_payload_t){
                .p0 = (uint8_t)(count / 256),
                .p1 = (uint8_t)(count % 256)
        });
}

proto_payload_t proto_make_devstatus_payload(uint8_t devs, pch_bsize_t esize);
proto_payload_t proto_make_start_payload(uint8_t ccwcmd, pch_bsize_t esize);
struct proto_parsed_devstatus_payload proto_parse_devstatus_payload(proto_payload_t p);

#endif
