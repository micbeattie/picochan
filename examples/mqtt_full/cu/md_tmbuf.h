/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _MD_TMBUF_H
#define _MD_TMBUF_H

#include <stdint.h>

#ifndef TMBUF_BUFFSIZE
#define TMBUF_BUFFSIZE          256
#endif

static_assert(TMBUF_BUFFSIZE >= 1 && TMBUF_BUFFSIZE <= 32767,
        "TMBUF_BUFFSIZE must be between 1 and 32767");

typedef struct tmbuf {
        uint8_t         flags;
        //! tlen is the length of the topic, not counting the
        //! always-present trailing \0
        uint8_t         tlen;
        //! mlen is the length of the message which starts
        //! immediately after the trailing \0 of the always-present
        //! topic and does not itself count or require a trailing \0.
        uint16_t        mlen;
        //! buf is of the form
        //! ['t','o','p','i','c','\0','m','e','s','s','a','g','e']
        //! followed by zero or more ignored bytes
        char            buf[TMBUF_BUFFSIZE];
} tmbuf_t;

static inline char *tmbuf_topic_ptr(tmbuf_t *tm) {
        return tm->buf;
}

static inline uint tmbuf_topic_maxlen(tmbuf_t *tm) {
        uint maxlen = sizeof(tm->buf) - 1;
        if (maxlen > 255)
                maxlen = 255;

        return maxlen;
}

static inline char *tmbuf_message_ptr(tmbuf_t *tm) {
        return tm->buf + tm->tlen + 1;
}

static inline uint tmbuf_message_replace_maxlen(tmbuf_t *tm) {
        return sizeof(tm->buf) - tm->tlen - 1;
}

static inline uint tmbuf_message_append_maxlen(tmbuf_t *tm) {
        return sizeof(tm->buf) - tm->tlen - 1 - tm->mlen;
}

static inline char *tmbuf_message_append_ptr(tmbuf_t *tm) {
        return tm->buf + tm->tlen + 1 + tm->mlen;
}

static inline void tmbuf_reset(tmbuf_t *tm) {
        tm->buf[0] = '\0';
        tm->tlen = 0;
        tm->mlen = 0;
}

static inline void tmbuf_reset_message(tmbuf_t *tm) {
        tm->mlen = 0;
}

bool tmbuf_parse(tmbuf_t *tm, uint count);
bool tmbuf_write_topic(tmbuf_t *tm, const char *topic);
bool tmbuf_write_message_replace(tmbuf_t *tm, const char *message, uint mlen);
bool tmbuf_write_message_append(tmbuf_t *tm, const char *message, uint mlen);

#endif
