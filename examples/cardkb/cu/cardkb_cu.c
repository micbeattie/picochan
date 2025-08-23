#include <string.h>
#include "hardware/i2c.h"
#include "pico/time.h"
#include "picochan/devib.h"
#include "picochan/dev_status.h"
#include "picochan/ccw.h"
#include "picochan/cu.h"

#include "../cardkb_api.h"

#ifndef CARDKB_MAX_NUM_DEVS
#define CARDKB_MAX_NUM_DEVS 2
#endif

static pch_dev_range_t cardkb_dev_range;
static alarm_pool_t *cardkb_alarm_pool; // Must run on same core as cardkb_cu
static repeating_timer_t cardkb_timer;

static pch_cbindex_t cardkb_start_cbindex;
static pch_cbindex_t cardkb_finish_cbindex;

// Poll for new key pressed every 50ms. CardKB keeps track of the
// ASCII-ish value of the latest key pressed (initially 0). An I2C
// read fetches that latest value and zeroes it out. Our repeating
// timer callback does not need to fire while a key is actually
// pressed - just fire often enough that more than one key is not
// pressed during that period. Given that CardKB is not exactly
// designed for high speed typing, 20 times a second should be fine.
#define CARDKB_TIMER_DELAY_MS 50

#ifndef CARDKB_DEV_BUFFSIZE
#define CARDKB_DEV_BUFFSIZE 64
#endif
// A count of CARDKB_DEV_BUFFSIZE+1 is used to mean "overrun" and must
// fit into a uint8_t
static_assert(CARDKB_DEV_BUFFSIZE <= 254,
        "CARDKB_DEV_BUFFSIZE must be at most 254");

typedef struct cardkb_dev {
        absolute_time_t         deadline;
        cardkb_dev_config_t     config;
        i2c_inst_t              *i2c;
        uint8_t                 i2c_addr; // I2C address
        uint8_t                 offset; // 0 or CARDKB_DEV_BUFFSIZE
        uint8_t                 count;  // #bytes at offset; buffsize+1=overrun
        bool                    reading;
        unsigned char           buf[2 * CARDKB_DEV_BUFFSIZE]; // Two buffers
} cardkb_dev_t;

cardkb_dev_t cardkb_devs[CARDKB_MAX_NUM_DEVS];

static inline cardkb_dev_t *get_cardkb_dev(pch_devib_t *devib) {
        int i = pch_dev_range_get_index_required(&cardkb_dev_range, devib);
        if (i >= 0)
                return &cardkb_devs[i];

        return NULL;
}

static inline pch_devib_t *cardkb_get_devib(cardkb_dev_t *cd) {
        return pch_dev_range_get_devib_by_index(&cardkb_dev_range,
                cd - cardkb_devs);
}

static void reset_cardkb_dev(cardkb_dev_t *cd) {
        memset(cd, 0, sizeof(*cd));
}

static uint8_t readkey_cardkb_dev(cardkb_dev_t *cd) {
        i2c_inst_t *i2c = cd->i2c;
        if (!i2c)
                return 0;

        uint8_t ch;
        if (i2c_read_blocking(i2c, cd->i2c_addr, &ch, 1, false) == 1)
                return ch;

        return 0;
}

static void cardkb_finish(pch_devib_t *devib) {
        pch_dev_update_status_ok_then(devib, cardkb_start_cbindex);
}

static void flip_cardkb_dev(cardkb_dev_t *cd) {
        if (cd->offset)
                cd->offset = 0;
        else
                cd->offset = CARDKB_DEV_BUFFSIZE;

        memset(cd->buf + cd->offset, 0, CARDKB_DEV_BUFFSIZE);
        cd->count = 0;
}

static void cardkb_dev_recalc_deadline(cardkb_dev_t *cd) {
        uint32_t timeout_cs = (uint32_t)cd->config.timeout_cs;

        if (timeout_cs == CARDKB_TIMEOUT_NEVER)
                cd->deadline = at_the_end_of_time;
        else {
                uint32_t delay_ms = timeout_cs * 10;
                absolute_time_t now = get_absolute_time();
                cd->deadline = delayed_by_ms(now, delay_ms);
        }
}

static void send_and_flip_dev(cardkb_dev_t *cd) {
        unsigned char *data = cd->buf + cd->offset;
        uint16_t n = (uint16_t)(cd->count);
        flip_cardkb_dev(cd);
        cardkb_dev_recalc_deadline(cd);
        cd->reading = false;

        pch_devib_t *devib = cardkb_get_devib(cd);
        if (n == 0) {
                uint8_t devs = PCH_DEVS_CHANNEL_END
                        | PCH_DEVS_DEVICE_END
                        | PCH_DEVS_UNIT_EXCEPTION;
                pch_dev_update_status_then(devib, devs,
                        cardkb_start_cbindex);
        } else {
                pch_dev_send_final_then(devib, data, n,
                        cardkb_start_cbindex);
        }
}

static bool cardkb_dev_ready(cardkb_dev_t *cd) {
        if (cd->count >= cd->config.minread)
                return true;

        if (get_absolute_time() >= cd->deadline)
                return true;

        unsigned char eol = cd->config.eol;
        if (!eol)
                return false;

        if (cd->count == 0)
                return false;

        return cd->buf[cd->offset + cd->count - 1] == eol;
}

static int do_cardkb_read(pch_devib_t *devib, cardkb_dev_t *cd) {
       assert(!cd->reading);
       if (cd->count > CARDKB_DEV_BUFFSIZE) {
                // overrun
                reset_cardkb_dev(cd);
                pch_dev_sense_t sense = {
                        .flags = PCH_DEV_SENSE_OVERRUN
                };
                pch_dev_update_status_error_then(devib, sense,
                        cardkb_start_cbindex);
                return 0;
        }

        cardkb_dev_recalc_deadline(cd);
        if (cardkb_dev_ready(cd))
                send_and_flip_dev(cd);
        else
                cd->reading = true;

        return 0;
}

static int do_cardkb_start(pch_devib_t *devib) {
        cardkb_dev_t *cd = get_cardkb_dev(devib);
        if (!cd)
                return -EINVALIDDEV;

        if (pch_devib_is_stopping(devib))
                return -ECANCEL;

        uint8_t ccwcmd = devib->payload.p0;
        switch (ccwcmd) {
        case PCH_CCW_CMD_READ:
                return do_cardkb_read(devib, cd);

        case CARDKB_CCW_CMD_GET_CONFIG:
                pch_dev_send_final_then(devib, &cd->config,
                        sizeof(cd->config), cardkb_start_cbindex);
                return 0;

        case CARDKB_CCW_CMD_SET_CONFIG:
                pch_dev_receive_then(devib, &cd->config,
                        sizeof(cd->config), cardkb_finish_cbindex);
                return 0;

        default:
                return -EINVALIDCMD;
        }

        // NOTREACHED
}

static void cardkb_start(pch_devib_t *devib) {
        pch_dev_call_or_reject_then(devib, do_cardkb_start,
                cardkb_start_cbindex);
}

static void cardkb_timer_callback_dev(cardkb_dev_t *cd) {
        uint8_t ch = readkey_cardkb_dev(cd);
        if (ch) {
                if (cd->count < CARDKB_DEV_BUFFSIZE)
                        cd->buf[cd->offset + cd->count] = ch;

                if (cd->count <= CARDKB_DEV_BUFFSIZE)
                        cd->count++; // 1 past buffsize means overrun
        }

        if (!cd->reading)
                return;

        if (cardkb_dev_ready(cd))
                send_and_flip_dev(cd);
}

static bool cardkb_timer_callback(repeating_timer_t *rt) {
        for (int i = 0; i < cardkb_dev_range.num_devices; i++)
                cardkb_timer_callback_dev(&cardkb_devs[i]);

        return true; // continue repeating
}

void cardkb_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices) {
        pch_dev_range_init(&cardkb_dev_range, cu, first_ua, num_devices);

        cardkb_start_cbindex =
                pch_register_unused_devib_callback(cardkb_start);
        cardkb_finish_cbindex =
                pch_register_unused_devib_callback(cardkb_finish);

        cardkb_alarm_pool = alarm_pool_create_with_unused_hardware_alarm(1);
        bool ok = alarm_pool_add_repeating_timer_ms(cardkb_alarm_pool,
                CARDKB_TIMER_DELAY_MS, cardkb_timer_callback, NULL,
                &cardkb_timer);
        hard_assert(ok);
}

void cardkb_dev_init(pch_unit_addr_t ua, i2c_inst_t *i2c, uint8_t i2c_addr) {
        pch_devib_t *devib = pch_dev_range_get_devib_by_ua_required(
                &cardkb_dev_range, ua);

        cardkb_dev_t *cd = get_cardkb_dev(devib);
        reset_cardkb_dev(cd);
        cd->i2c_addr = i2c_addr;
        cd->i2c = i2c;

        pch_dev_set_callback(devib, cardkb_start_cbindex);
}
