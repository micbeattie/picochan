/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "pico.h"
#include "payload.h"

struct proto_parsed_devstatus_payload proto_parse_devstatus_payload(proto_payload_t p) {
        return ((struct proto_parsed_devstatus_payload){
                .count = pch_bsize_decode_raw(p.p1),
                .devs = p.p0
        });
}

proto_payload_t __time_critical_func(proto_make_devstatus_payload)(uint8_t devs, pch_bsize_t esize) {
        return ((proto_payload_t){
                .p0 = devs,
                .p1 = pch_bsize_unwrap(esize)
        });
}

proto_payload_t __time_critical_func(proto_make_start_payload)(uint8_t ccwcmd, pch_bsize_t esize) {
        return ((proto_payload_t){
                .p0 = ccwcmd,
                .p1 = pch_bsize_unwrap(esize)
        });
}
