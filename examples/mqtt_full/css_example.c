/*
 * Copyright (c) 2026 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include <stdio.h>
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"

#include "picochan/css.h"
#include "picochan/dev_status.h"

#include "../mqtt_api.h"

// PRINT_MESSAGES_GPIO is used as an input GPIO. When 1, print
// details of incoming messages during the I/O callback
#define PRINT_MESSAGES_GPIO 20

// STATS_GPIO is used as an input GPIO. On a low-to-high edge,
// print MQTT message statistics
#define STATS_GPIO 21

static const char mqtt_hostname[] = MQTT_SERVER_HOST;
static const char mqtt_username[] = MQTT_USERNAME;
static const char mqtt_password[] = MQTT_PASSWORD;
static const uint16_t mqtt_port = MQTT_SERVER_PORT;
static const char topic[] = "pico/output";
static const char message[] = "Hello world";
static const char extra[] = " again";

static const char cats_topic[] = "cats";
static const char dogs_topic[] = "dogs";

static uint16_t cats_filter_id = 1;
static uint16_t dogs_filter_id = 2;

#define CCMD(suffix) PCH_CCW_CMD_ ## suffix
#define MCMD(suffix) MQTT_CCW_CMD_ ## suffix
#define FL(suffix) PCH_CCW_FLAG_ ## suffix
#define ABUF(t) sizeof(t) , (uint32_t)t
#define ASTR(t) strlen(t) , (uint32_t)t
#define AOBJ(t) sizeof(t) , (uint32_t)&t
#define A0 0, 0

static pch_ccw_t prepare_chanprog[] = {
        { MCMD(SET_MQTT_HOSTNAME), FL(CC)|FL(SLI), ASTR(mqtt_hostname) },
        { MCMD(SET_MQTT_USERNAME), FL(CC)|FL(SLI), ASTR(mqtt_username) },
        { MCMD(SET_MQTT_PASSWORD), FL(CC)|FL(SLI), ASTR(mqtt_password) },
        { MCMD(SET_MQTT_PORT),     FL(CC),         AOBJ(mqtt_port) },
        { MCMD(CONNECT),           FL(CC),         A0 },
        { MCMD(WRITE_TOPIC),       FL(CC)|FL(SLI), ASTR(topic) },
        { MCMD(WRITE_MESSAGE),     FL(CC)|FL(SLI), ASTR(message) },
        { MCMD(PUBLISH),           FL(CC),         A0 },
        { MCMD(WRITE_MESSAGE_APPEND), FL(CC)|FL(SLI), ASTR(extra) },
        { MCMD(PUBLISH),           FL(CC),         A0 },
        { MCMD(SET_CURRENT_ID),    FL(CC)|FL(SLI), AOBJ(cats_filter_id) },
        { MCMD(WRITE_TOPIC),       FL(CC)|FL(SLI), ASTR(cats_topic) },
        { MCMD(SUBSCRIBE),         FL(CC),         A0 },
        { MCMD(SET_CURRENT_ID),    FL(CC)|FL(SLI), AOBJ(dogs_filter_id) },
        { MCMD(WRITE_TOPIC),       FL(CC)|FL(SLI), ASTR(dogs_topic) },
        { MCMD(SUBSCRIBE),         0,              A0 }
};

static md_ring_t cats_ring = { .start = 4, .next = 4, .end = 33 };
static md_ring_t dogs_ring = { .start = 34, .next = 34, .end = 63 };

static pch_ccw_t prepare_cats_chanprog[] = {
        { MCMD(SET_FILTER_ID),     FL(CC),         AOBJ(cats_filter_id) },
        { MCMD(SET_CURRENT_ID),    FL(CC),         AOBJ(cats_ring.start) },
        { MCMD(SET_RING),          FL(CC),         AOBJ(cats_ring) },
        { MCMD(START_RING),        0,              A0 }
};

static char cats_message[256];

static pch_ccw_t follow_cats_chanprog[] = {
        { MCMD(ACK),               FL(CC),         A0 },
        { MCMD(WAIT),              FL(CC),         A0 },
        { MCMD(READ_MESSAGE),      FL(SLI),        ABUF(cats_message) }
};

static pch_ccw_t prepare_dogs_chanprog[] = {
        { MCMD(SET_FILTER_ID),     FL(CC),         AOBJ(dogs_filter_id) },
        { MCMD(SET_CURRENT_ID),    FL(CC),         AOBJ(dogs_ring.start) },
        { MCMD(SET_RING),          FL(CC),         AOBJ(dogs_ring) },
        { MCMD(START_RING),        0,              A0 }
};

static char dogs_message[256];

static pch_ccw_t follow_dogs_chanprog[] = {
        { MCMD(ACK),               FL(CC),         A0 },
        { MCMD(WAIT),              FL(CC),         A0 },
        { MCMD(READ_MESSAGE),      FL(SLI),        ABUF(dogs_message) }
};

const pch_sid_t cats_sid = 1;
const pch_sid_t dogs_sid = 2;

uint32_t io_cb_count;
uint32_t io_cb_count_cats;
uint32_t io_cb_count_dogs;

static void print_stats(void) {
        printf("io_cb_count       = %lu\n", io_cb_count);
        printf("io_cb_count_cats  = %lu\n", io_cb_count_cats);
        printf("io_cb_count_dogs  = %lu\n", io_cb_count_dogs);
}

// Printing "too much" on a line to USB stdio from a callback results
// in dropped characters.
#define MAX_MESSAGE_PRINT_LEN 48
static void print_message_extract(const char *s, int len) {
        if (len <= MAX_MESSAGE_PRINT_LEN) {
                // no problem
                stdio_put_string(s, len, true, true);
                return;
        }

        const int slen = (MAX_MESSAGE_PRINT_LEN-1)/2;
        stdio_put_string(s, slen, false, false);
        stdio_put_string("...", 3, false, false);
        stdio_put_string(s + len - slen, slen, true, true);
}

void io_cb(pch_intcode_t ic, pch_scsw_t scsw) {
        pch_sid_t sid = ic.sid;
        assert(ic.cc == 1);
        if (scsw.schs) {
                printf("Unexpected subchannel status %02x for SID %u\n",
                        scsw.schs, sid);
                return;
        }

        if (pch_dev_status_unusual(scsw.devs)) {
                printf("Unusual device status 0x%02x for SID %u\n",
                        scsw.devs, sid);
                return;
        }

        io_cb_count++;

        bool do_print_messages = !gpio_get(PRINT_MESSAGES_GPIO);
        uint len;
        int cc;

        switch (sid) {
        case cats_sid:
                io_cb_count_cats++;
                if (do_print_messages) {
                        len = sizeof(cats_message) - scsw.count;
                        printf("Received cats message length %u: ", len);
                        print_message_extract(cats_message, len);
                }
                cc = pch_sch_start(cats_sid, follow_cats_chanprog);
                assert(!cc);
                break;

        case dogs_sid:
                io_cb_count_dogs++;
                if (do_print_messages) {
                        len = sizeof(dogs_message) - scsw.count;
                        printf("Received dogs message length %u: ", len);
                        print_message_extract(dogs_message, len);
                }
                cc = pch_sch_start(dogs_sid, follow_dogs_chanprog);
                assert(!cc);
                break;

        default:
                printf("io_cb: Unexpected SID %u\n", sid);
                break;
        }
}

static bool do_print_stats;

static void stats_gpio_irq_cb(uint gpio, uint32_t event_mask) {
        do_print_stats = true;
}

void run_css_example(void) {
        printf("enabling input from GPIO %u - on click (falling edge) will print message statistics\n",
                STATS_GPIO);
        gpio_init(STATS_GPIO);
        gpio_set_irq_enabled_with_callback(STATS_GPIO,
                GPIO_IRQ_EDGE_FALL, true, stats_gpio_irq_cb);

        printf("enabling input from GPIO %u - while pressed (1), I/O callback will print incoming messages\n",
                PRINT_MESSAGES_GPIO);
        gpio_init(PRINT_MESSAGES_GPIO);

        printf("running synchronous channel program to connect and publish to MQTT topic \"pico/output\"\n");
        pch_sch_run_wait(0, prepare_chanprog, NULL);

        printf("running prepare_cats_chanprog on SID %u to follow topic \"cats\"\n",

                cats_sid);
        pch_sch_run_wait(cats_sid, prepare_cats_chanprog, NULL);

        printf("running prepare_dogs_chanprog on SID %u to follow topic \"dogs\"\n",
                dogs_sid);
        pch_sch_run_wait(dogs_sid, prepare_dogs_chanprog, NULL);

        // Enable callbacks for schibs in ISC 0 (the default)
        pch_css_set_isc_enabled(0, true);
        printf("starting follow_cats_chanprog (without initial ack) to wait/read/ack messages published to topic \"cats\"\n");
        pch_sch_start(cats_sid, &follow_cats_chanprog[1]);

        printf("starting follow_dogs_chanprog (without initial ack) to wait/read/ack messages published to topic \"dogs\"\n");
        pch_sch_start(dogs_sid, &follow_dogs_chanprog[1]);
        printf("started follow_dogs_chanprog ok\n");

        printf("About to do loop with __wfe() and STATS_GPIO\n");

        while (1) {
                __wfe();
                if (do_print_stats) {
                        print_stats();
                        do_print_stats = false;
                }
        }
}
