/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_PROTO_PACKET_H
#define _PCH_PROTO_PACKET_H

#include <assert.h>
#include "chop.h"
#include "payload.h"
#include "picochan/ids.h"
#include "picochan/bsize.h"

/*! \file proto/packet.h
 *  \defgroup internal_proto internal_proto
 *
 * \brief The internal protocol between CSS and CU
 */

/*! \brief a 4-byte command packet sent on a channel between CSS and CU or vice versa
 *  \ingroup picochan_proto
 *
 * Various parts of this implementation are tuned for and rely on the
 * size being exactly 4 bytes. Note that the ARM ABI specifies that a
 * return value of a composite type of up to 4 bytes (such as
 * proto_packet_t) is passed in R0, thus behaving the same way as a
 * 32-bit return value.
 */
typedef struct __attribute__((aligned(4))) proto_packet {
        proto_chop_t    chop;
        pch_unit_addr_t unit_addr;
        uint8_t         p0;
        uint8_t         p1;
} proto_packet_t;

static_assert(sizeof(proto_packet_t) == 4, "proto_packet_t must be 4 bytes");

static inline proto_payload_t proto_get_payload(proto_packet_t p) {
        return ((proto_payload_t){p.p0, p.p1});
}

static inline uint32_t proto_packet_as_word(proto_packet_t p) {
        return *(uint32_t *)&p;
}

// proto_get_count parses the payload of the packet as a 2-byte
// big-endian value
static inline uint16_t proto_get_count(proto_packet_t p) {
        return ((uint16_t)p.p0 << 8) + (uint16_t)p.p1; // big endian
}

// proto_decode_esize_payload decodes the second byte of the payload
// of the packet (p.p1), treating it as a pch_decode_bsize and using
// pch_bsize_decode_raw to return the resulting count. 
static inline uint16_t proto_decode_esize_payload(proto_packet_t p) {
        return pch_bsize_decode_raw(p.p1);
}

static inline proto_packet_t proto_make_packet(proto_chop_t chop, pch_unit_addr_t ua, proto_payload_t payload) {
        return ((proto_packet_t){
                .chop = chop,
                .unit_addr = ua,
                .p0 = payload.p0,
                .p1 = payload.p1
        });
}

static inline proto_packet_t proto_make_count_packet(proto_chop_t chop, pch_unit_addr_t ua, uint16_t count) {
        return ((proto_packet_t){
                .chop = chop,
                .unit_addr = ua,
                .p0 = count / 256, // big-endian: high byte
                .p1 = count % 256  // big-endian: low byte
        });
}

static inline proto_packet_t proto_make_esize_packet(proto_chop_t chop, pch_unit_addr_t ua, uint8_t p0, pch_bsize_t esize) {
        return ((proto_packet_t){
                .chop = chop,
                .unit_addr = ua,
                .p0 = p0,
                .p1 = pch_bsize_unwrap(esize)
        });
}

#endif
