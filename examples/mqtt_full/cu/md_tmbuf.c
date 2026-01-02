/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "mqtt_cu_internal.h"
#include "md_tmbuf.h"

bool tmbuf_parse(tmbuf_t *tm, uint count) {
        uint tlen = 0; // 0 is an invalid topic length
        const char *p = memchr(tm->buf, 0, count);
        if (p)
                tlen = p - tm->buf; 

        uint mlen = count - tlen - 1; // meaningless if tlen == 0
        if (tlen == 0 || tlen > tmbuf_topic_maxlen(tm) || mlen == 0)
                return false;

        tm->tlen = (uint8_t)tlen;
        tm->mlen = (uint16_t)mlen;
        return true;
}

bool tmbuf_write_topic(tmbuf_t *tm, const char *topic) {
        size_t tlen = strlen(topic);
        if (tlen >= 256 || tlen >= sizeof(tm->buf))
                return false;

        tm->tlen = (uint8_t)tlen;
        tm->mlen = 0; // resets message to empty
        memcpy(tm->buf, topic, tlen);
        tm->buf[tlen] = '\0'; // explicitly add trailing \0
        return true;
}

bool tmbuf_write_message_replace(tmbuf_t *tm, const char *message, uint mlen) {
        uint moffset = tm->tlen + 1;
        if (moffset + mlen > sizeof(tm->buf))
                return false;

        // copy the message after the trailing \0 of the topic
        memcpy(tm->buf + moffset, message, mlen);
        tm->mlen = mlen;
        return true;
}

bool tmbuf_write_message_append(tmbuf_t *tm, const char *message, uint mlen) {
        uint moffset = tm->tlen + 1 + tm->mlen;
        if (moffset + mlen > sizeof(tm->buf))
                return false;

        // copy the message after the previous message
        memcpy(tm->buf + moffset, message, mlen);
        tm->mlen += mlen;
        return true;
}
