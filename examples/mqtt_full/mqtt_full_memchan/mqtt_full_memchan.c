/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include "pico/stdio.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/status_led.h"
#include "pico/cyw43_arch.h"

#include "picochan/css.h"
#include "picochan/cu.h"

// PRINT_MESSAGES_GPIO is used as an input GPIO. When 1, print
// details of incoming messages during the I/O callback
#define PRINT_MESSAGES_GPIO 20

// STATS_GPIO is used as an input GPIO. On a low-to-high edge,
// print MQTT CU statistics
#define STATS_GPIO 21

#define NUM_MQTT_DEVS 8

#include "../mqtt_cu_api.h"

/*
 * mqtt_full_memchan runs the complete mqtt Picochan example on a
 * single Pico. The CSS is run on core 0 and the CU on core 1.
 * Instead of needing physical channel connections between CSS
 * and CU, this configuration uses a memory channel (memchan)
 * so that CSS-to-CU communication happens directly via
 * memory-to-memory DMA for data transfers and 4-byte
 * writes/reads from memory for command transfers.
 */

const pch_unit_addr_t FIRST_UA = 0;
const pch_cuaddr_t CUADDR = 0;
const pch_chpid_t CHPID = 0;

#define MQTT_ENABLE_TRACE true

#ifdef MQTT_ENABLE_TRACE
//#define MQTT_CU_TRACE_FLAGS PCH_CU_TRACED_MASK
//#define MQTT_CHP_TRACE_FLAGS PCH_CHP_TRACED_MASK
#define MQTT_CU_TRACE_FLAGS PCH_CU_TRACED_GENERAL
#define MQTT_CHP_TRACE_FLAGS PCH_CHP_TRACED_GENERAL
#else
#define MQTT_CU_TRACE_FLAGS 0
#define MQTT_CHP_TRACE_FLAGS 0
#endif

static pch_cu_t mqtt_cu = PCH_CU_INIT(NUM_MQTT_DEVS);

static void light_led_ms(uint32_t ms) {
        status_led_init_with_context(cyw43_arch_async_context());
        status_led_set_state(true);
        sleep_ms(ms);
        status_led_set_state(false);
}

static void wifi_connect(void) {
        cyw43_arch_enable_sta_mode();

        printf("connecting to WiFi...\n");
        int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID,
                WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000);
        if (err != 0) {
                printf("connect to WiFi failed: err=%d\n", err);
                panic("WiFi connect failed");
        }

        printf("connected to WiFi\n");
}

bool core1_ready;

static void core1_thread(void) {
        int err = cyw43_arch_init();
        if (err != 0)
                panic("cyw43_arch_init");

        light_led_ms(1000);

        wifi_connect();
        pch_cus_init(); // could do from core 0
        pch_cus_set_trace(MQTT_ENABLE_TRACE); // could do from core 0
        
        mqtt_cu_init(&mqtt_cu, FIRST_UA, NUM_MQTT_DEVS);
        pch_cu_register(&mqtt_cu, CUADDR);
        pch_cu_set_trace_flags(CUADDR, MQTT_CU_TRACE_FLAGS);

        pch_channel_t *chpeer = pch_chp_get_channel(CHPID);
        pch_cus_memcu_configure(CUADDR, chpeer);

        pch_cu_start(CUADDR);

        printf("CU ready\n");
        core1_ready = true; // core0 waits for this

        while (1) {
                mqtt_cu_poll();
                sleep_ms(5); // XXX help or hinder?
        }
}

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
        extern md_cu_stats_t md_cu_statistics;

        printf("io_cb_count       = %lu\n", io_cb_count);
        printf("io_cb_count_cats  = %lu\n", io_cb_count_cats);
        printf("io_cb_count_dogs  = %lu\n", io_cb_count_dogs);
        printf("task_success      = %lu\n",
		md_cu_statistics.task_success);
        printf("task_pause        = %lu\n",
		md_cu_statistics.task_pause);
        printf("task_restart      = %lu\n",
		md_cu_statistics.task_restart);
        printf("oversize_topic    = %lu\n",
		md_cu_statistics.oversize_topic);
        printf("oversize_message  = %lu\n",
		md_cu_statistics.oversize_message);
        printf("received_success  = %lu\n",
		md_cu_statistics.received_success);
        printf("received_overflow = %lu\n",
		md_cu_statistics.received_overflow);
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

int main(void) {
        bi_decl(bi_program_description("picochan mqtt_full memchan CSS+CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();
        sleep_ms(2000);
        printf("started main on core0\n");

        pch_memchan_init();

        pch_css_init();
        pch_css_set_trace(MQTT_ENABLE_TRACE);
        pch_css_start(io_cb, 0); // start with callbacks disabled for all ISCs
        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 3); // allocates SIDs 0-2
        pch_chp_set_trace_flags(chpid, MQTT_CHP_TRACE_FLAGS);

        printf("starting core1 and waiting for it to be ready...\n");
        multicore_launch_core1(core1_thread);
        while (!core1_ready)
                sleep_ms(1);

        printf("core0 continuing\n");

        pch_channel_t *chpeer = pch_cu_get_channel(CUADDR);
        pch_chp_configure_memchan(CHPID, chpeer);

        for (pch_sid_t sid = 0; sid < 3; sid++) {
                pch_sch_modify_enabled(sid, true);
                pch_sch_modify_traced(sid, MQTT_ENABLE_TRACE);
        }

        pch_chp_start(chpid);

        printf("enabling input from GPIO %u - when 1, I/O callback will print incoming messages\n",
                PRINT_MESSAGES_GPIO);

        gpio_init(PRINT_MESSAGES_GPIO);
        gpio_init(STATS_GPIO);
        gpio_set_irq_enabled_with_callback(STATS_GPIO,
                GPIO_IRQ_EDGE_FALL, true, stats_gpio_irq_cb);

        printf("running synchronous channel program to connect and publish to MQTT\n");
        pch_sch_run_wait(0, prepare_chanprog, NULL);

        printf("running prepare_cats_chanprog on SID %u to follow cats topic\n",

                cats_sid);
        pch_sch_run_wait(cats_sid, prepare_cats_chanprog, NULL);

        printf("running prepare_dogs_chanprog on SID %u to follow dogs topic\n",
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

        // Test tracing
#define MD_TRC_RT_APP_GPIOS 201
        pch_css_trace_write_user(MD_TRC_RT_APP_GPIOS,
                ((uint8_t[2]){PRINT_MESSAGES_GPIO, STATS_GPIO}), 2);

        while (1) {
                __wfe();
                if (do_print_stats) {
                        print_stats();
                        do_print_stats = false;
                }
        }
}
