/*
 * Copyright (c) 2025-2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "mqtt_cu_internal.h"

// For now, md_topic_filter_match is just an exact match, not an
// MQTT wildcard, not a glob and not a regexp.
static bool md_topic_filter_match(const char *topic_filter, const char *topic) {
        return !strcmp(topic_filter, topic);
}

static void topic_cb(mqtt_cu_config_t *cfg, mqtt_dev_t *md, const char *topic, u32_t tot_len) {
        if (!md_ring_is_started(md))
                return;

        assert(md_ring_valid(&md->ring));
        tmbuf_t *filt_tm = get_tmbuf(cfg, md->filt);
        if (!filt_tm)
                return;

        tmbuf_t *tm = get_tmbuf(cfg, md->ring.next);
        const char *topic_filter = tmbuf_topic_ptr(filt_tm);
        if (!md_topic_filter_match(topic_filter, topic)) {
                tmbuf_reset(tm);
                return;
        }

        if (!tmbuf_write_topic(tm, topic)) {
                md_cu_statistics.oversize_topic++;
                return;
        }

        if (tot_len > tmbuf_message_replace_maxlen(tm)) {
                // Message would be too big. Clear topic so
                // md_inpub_data_cb() will not even try writing to it
                md_cu_statistics.oversize_message++;
                tmbuf_reset(tm);
                return;
        }
}

void md_inpub_start_cb(void *arg, const char *topic, u32_t tot_len) {
        mqtt_cu_config_t *cfg = arg;

        uint16_t num_devices = cfg->hldev_config.dev_range.num_devices;
        for (int i = 0; i < num_devices; i++) {
                mqtt_dev_t *md = &cfg->mds[i];
                topic_cb(cfg, md, topic, tot_len);
        }
}

static bool message_receive_complete(mqtt_dev_t *md) {
        md_ring_t *mr = &md->ring;

        uint32_t status = md_ring_lock();
        uint16_t cur = md->cur;
        uint16_t next = mr->next;
        uint16_t new_next = md_ring_increment(mr, next);
        mr->next = new_next;
        bool wake = (next == cur);
        bool full = (new_next == cur);
        if (full)
                mr->full = next;
        md_ring_unlock(status);

        if (full)
                md_cu_statistics.received_overflow++;
        else
                md_cu_statistics.received_success++;

        return wake;
}

static void message_cb(mqtt_cu_config_t *cfg, mqtt_dev_t *md, const u8_t *data, uint16_t len, uint8_t flags) {
        if (!md_ring_is_started(md))
                return;

        md_ring_t *mr = &md->ring;
        assert(md_ring_valid(mr));
        if (md_ring_full(mr))
                return;

        uint16_t next = mr->next;
        tmbuf_t *tm = get_tmbuf(cfg, next);
        if (!tm->tlen)
                return;

        if (!tmbuf_write_message_append(tm, (char*)data, len)) {
                tmbuf_reset(tm);
                return;
        }

        if (flags & MQTT_DATA_FLAG_LAST) {
                bool wake = message_receive_complete(md);
                if (wake)
                        md_wake(cfg, md);
        }
}

void md_inpub_data_cb(void *arg, const u8_t *data, uint16_t len, uint8_t flags) {
        mqtt_cu_config_t *cfg = arg;

        uint16_t num_devices = cfg->hldev_config.dev_range.num_devices;
        for (int i = 0; i < num_devices; i++) {
                mqtt_dev_t *md = &cfg->mds[i];
                message_cb(cfg, md, data, len, flags);
        }
}
