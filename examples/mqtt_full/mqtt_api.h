/*
 * Copyright (c) 2025-2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#ifndef _MQTT_API_H
#define _MQTT_API_H

#ifndef MQTT_HOSTNAME_MAXLEN
#define MQTT_HOSTNAME_MAXLEN    63
#endif

#ifndef MQTT_USERNAME_MAXLEN
#define MQTT_USERNAME_MAXLEN    31
#endif

#ifndef MQTT_PASSWORD_MAXLEN
#define MQTT_PASSWORD_MAXLEN    31
#endif

#ifndef MQTT_CLIENT_ID_MAXLEN
#define MQTT_CLIENT_ID_MAXLEN   31
#endif

#ifndef MQTT_TOPIC_MAXLEN
#define MQTT_TOPIC_MAXLEN       255
#endif

#ifndef MQTT_MESSAGE_MAXLEN
#define MQTT_MESSAGE_MAXLEN     256
#endif

#ifndef DEFAULT_MQTT_PORT
#define DEFAULT_MQTT_PORT       1883
#endif

typedef struct __attribute__((__packed__)) md_ring {
        uint16_t        start;   // start index of input ring
        uint16_t        next;    // cursor index in input ring
        uint16_t        end;     // end index of input ring
        uint16_t        full;    // where ring became full or MD_RING_NOT_FULL
} md_ring_t;

#define MD_RING_NOT_FULL 0xffff

static inline bool md_ring_contains(md_ring_t *mr, uint16_t n) {
        return mr->start <= n && n < mr->end;
}

static inline bool md_ring_full(md_ring_t *mr) {
        return mr->full != MD_RING_NOT_FULL;
}

static inline bool md_ring_valid(md_ring_t *mr) {
        return md_ring_contains(mr, mr->next);
}

typedef struct md_cu_stats {
        uint32_t        task_success;
        uint32_t        task_pause;
        uint32_t        task_restart;
        uint32_t        oversize_topic;
        uint32_t        oversize_message;
        uint32_t        received_success;
        uint32_t        received_overflow;
} md_cu_stats_t;

// Read CCWs

// read topic and message from tmbufs[cur] (topic\0message)
// same as PCH_CCW_CMD_READ
#define MQTT_CCW_CMD_READ_TOPIC_AND_MESSAGE 0x02

// read message from tmbufs[cur]
#define MQTT_CCW_CMD_READ_MESSAGE       0x04

// read topic from tmbufs[cur]
#define MQTT_CCW_CMD_READ_TOPIC         0x06

// wait until ring.next != cur
#define MQTT_CCW_CMD_WAIT               0x08

// cur++
#define MQTT_CCW_CMD_ACK                0x0a

// read data from ring
#define MQTT_CCW_CMD_GET_RING           0x0c

// Read CCWs which do not touch data (so could equally be Write)

// subscribe tmbufs[cur].topic
#define MQTT_CCW_CMD_SUBSCRIBE            0x20

// unsubscribe tmbufs[cur].topic
#define MQTT_CCW_CMD_UNSUBSCRIBE          0x22

// publish tmbufs[cur]
#define MQTT_CCW_CMD_PUBLISH              0x24

// mqtt_connect
#define MQTT_CCW_CMD_CONNECT              0x26

// mqtt_disconnect
#define MQTT_CCW_CMD_DISCONNECT           0x28

// start receiving filtered published messages into ring
#define MQTT_CCW_CMD_START_RING           0x2a

// stop receiving filtered published messages into ring
#define MQTT_CCW_CMD_STOP_RING            0x2c

// StatusModifier if messages match as glob: tmbufs[mc->cur] ~ tmbufs[n]
#define MQTT_CCW_CMD_MATCH_MESSAGE_ID0        0x80

// MATCH_MESSAGE_ID(n) valid for n from 0 to 7
// corresponding to CCWs 0x80, 0x82, ..., 0x8e
#define MQTT_CCW_CMD_MATCH_MESSAGE_ID(n) (MQTT_CCW_CMD_MATCH_MESSAGE_ID0 + 2 * (n))

// Write CCWs

// publish from data parsed as topic\0message
// same as PCH_CCW_CMD_WRITE
#define MQTT_CCW_CMD_WRITE_TOPIC_AND_MESSAGE 0x01

// write data to topic in tmbufs[cur] (resets message)
#define MQTT_CCW_CMD_WRITE_TOPIC             0x03

// write data to message in tmbufs[cur] (overwrite existing message)
#define MQTT_CCW_CMD_WRITE_MESSAGE           0x05

// write data to append to message in tmbufs[cur]
#define MQTT_CCW_CMD_WRITE_MESSAGE_APPEND    0x07

// cur = uint16_t from data
#define MQTT_CCW_CMD_SET_CURRENT_ID          0x09

// filt = uint16_t from data
#define MQTT_CCW_CMD_SET_FILTER_ID           0x0b

// write data to filter ring configuration (must be stopped)
#define MQTT_CCW_CMD_SET_RING                0x0d

// StatusModifier if messages match as glob: tmbufs[mc->cur] ~ mbufs[n] with uint16_t n from data
#define MQTT_CCW_CMD_MATCH_MESSAGE           0x0f

// Update global CU configuration. Can issue on any device but if
// another global configuration channel program is in progress then...
// TODO ...fails with COMMAND_REJECT with sense code ECUBUSY

// set cfg->mqtt_hostname from data
#define MQTT_CCW_CMD_SET_MQTT_HOSTNAME       0x21

// set cfg->mqtt_port from uint16_t data
#define MQTT_CCW_CMD_SET_MQTT_PORT           0x23

// set cfg->mqtt_username from data
#define MQTT_CCW_CMD_SET_MQTT_USERNAME       0x25

// set cfg->mqtt_password from data
#define MQTT_CCW_CMD_SET_MQTT_PASSWORD       0x27

// set cfg->mqtt_client_id from data
#define MQTT_CCW_CMD_SET_MQTT_CLIENT_ID      0x29

// Error numbers avoid those from picochan/dev_api.h and are used
// as sense code values for CCWs which result in COMMAND_REJECT
enum {
        MD_ERR_INVALID_TMBUF          = 128,
        MD_ERR_RING_STARTED           = 129,
        MD_ERR_RING_NOT_STARTED       = 130,
        MD_ERR_RING_INVALID           = 131,
        MD_ERR_CURSOR_OUT_OF_RING     = 132,
        MD_ERR_CU_BUSY                = 133,
        MD_ERR_NO_TOPIC               = 134
};
#endif
