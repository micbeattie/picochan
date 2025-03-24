/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_PROTO_CHOP_H
#define _PCH_PROTO_CHOP_H

// proto_chop_t represents a channel operation in a packet sent
// between CSS and CU in either direction.
// It is 8 bits with the top 4 as flag bits (with only 3 currently
// in use) and the bottom 4 as the operation command itself.
// The meaning of the flag bits depends on the operation command.
typedef uint8_t proto_chop_t;

typedef enum __packed proto_chop_cmd {
        PROTO_CHOP_START                = 0,
        PROTO_CHOP_ROOM                 = 1,
        PROTO_CHOP_DATA                 = 2,
        PROTO_CHOP_UPDATE_STATUS        = 3,
        PROTO_CHOP_REQUEST_READ         = 4
} proto_chop_cmd_t;
static_assert(sizeof(proto_chop_cmd_t) == 1, "proto_chop_cmd_t must be 1 byte");

typedef uint8_t proto_chop_flags_t;

// PROTO_CHOP_FLAG_SKIP is valid in CSS -> CU Room, Data and Start
// and in CU -> CSS Data
#define PROTO_CHOP_FLAG_SKIP    0x80

// PROTO_CHOP_FLAG_END and PROTO_CHOP_FLAG_STOP are valid in
// CSS -> CU Data
#define PROTO_CHOP_FLAG_END     0x40
#define PROTO_CHOP_FLAG_STOP    0x80

// PROTO_CHOP_FLAG_RESPONSE_REQUIRED is valid in CU -> CSS Data
#define PROTO_CHOP_FLAG_RESPONSE_REQUIRED     0x40

static inline proto_chop_flags_t proto_chop_flags(proto_chop_t c) {
        return (proto_chop_flags_t)(c & 0xf0);
}

static inline proto_chop_cmd_t proto_chop_cmd(proto_chop_t c) {
        return (proto_chop_cmd_t)(c & 0x0f);
}

#endif
