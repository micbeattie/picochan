#include <string.h>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "picochan/cu.h"
#include "picochan/dev_status.h"
#include "picochan/ccw.h"
#include "gd_debug.h"
#include "gd_cu.h"
#include "gd_config.h"
#include "gd_pins.h"

static pch_cu_t gd_cu = PCH_CU_INIT(NUM_GPIO_DEVS);
static bool gd_cu_done_init = false;

pch_cu_t *gd_get_cu() {
        return &gd_cu;
}

static alarm_pool_t *gd_alarm_pool; // Must run on same core as gd_cu

static pch_cbindex_t gd_start_cbindex;
static pch_cbindex_t gd_setconf_cbindex;
static pch_cbindex_t gd_write_cbindex;
static pch_cbindex_t gd_complete_test_cbindex;

gpio_dev_t gpio_devs[NUM_GPIO_DEVS];

static inline gpio_dev_t *get_gpio_dev(pch_unit_addr_t ua) {
        if (ua < NUM_GPIO_DEVS)
                return &gpio_devs[ua];

        return NULL;
}

static void reset_gpio_dev(gpio_dev_t *gd, pch_unit_addr_t ua) {
        memset(gd, 0, sizeof(*gd));
}

static void gd_add_repeating_timer(gpio_dev_t *gd, repeating_timer_callback_t callback, pch_unit_addr_t ua) {
        // Negate delay time so that delay is measured between start
        // (not end) of one callback and the next.
        int64_t delay_us = -(int64_t)gd->cfg.clock_period_us;

        void *user_data = (void*)(uint32_t)ua;
        bool ok = alarm_pool_add_repeating_timer_us(gd_alarm_pool,
                 delay_us, callback, user_data, &gd->rt);
        assert(ok); // alarm slots available to create the timer?
}

// CCW command implementations

static int do_ccw_get_config(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, void *data, size_t size) {
        if (n > size)
                n = size;

        return pch_dev_send_final_then(cu, ua, data, n,
                gd_start_cbindex);
}

static int do_ccw_set_config(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t room, gpio_dev_t *gd, uint8_t ccwcmd, size_t cfgsize) {
        if (room < cfgsize)
                return -EBUFFERTOOSHORT;

        gd->cfgcmd = ccwcmd;
        pch_dev_receive_then(cu, ua, &gd->cfgbuf, (uint16_t)cfgsize,
                gd_setconf_cbindex);

        return 0;
}

static int setconf_clock_period_us(gpio_dev_t *gd) {
        gd->cfg.clock_period_us = *(uint32_t *)gd->cfgbuf;
        return 0;
}

static int setconf_out_pins(gpio_dev_t *gd) {
        gd_pins_t *p = (gd_pins_t *)gd->cfgbuf;
        if (p->base > 31 || p->count > 7)
                return -EINVALIDVALUE;

        gd->cfg.out_pins = *p;
        return 0;
}

static int setconf_in_pins(gpio_dev_t *gd) {
        gd_pins_t *p = (gd_pins_t *)gd->cfgbuf;
        if (p->base > 31 || p->count > 7)
                return -EINVALIDVALUE;

        gd->cfg.in_pins = *p;
        return 0;
}

static int setconf_filter(gpio_dev_t *gd) {
        gd->cfg.filter = *(gd_filter_t *)gd->cfgbuf;
        return 0;
}

static int setconf_irq_config(gpio_dev_t *gd) {
        gd_irq_t *p = (gd_irq_t *)gd->cfgbuf;
        if (p->pin > 31 || (p->flags & ~GD_IRQ_FLAGS_MASK))
                return -EINVALIDVALUE;

        gd->cfg.irq = *p;
        return 0;
}

// do_gd_setconf is called from the devib's gd_setconf callback from
// the CU after a do_ccw_set_config has received the config data from
// the channel and written it to the cfgbuf. The CCW cmd is cfgcmd.
static int do_gd_setconf(pch_cu_t *cu, pch_unit_addr_t ua) {
        gpio_dev_t *gd = get_gpio_dev(ua);
        if (!gd)
                return -EINVALIDDEV;

        switch (gd->cfgcmd) {
        case GD_CCW_CMD_SET_CLOCK_PERIOD_US:
                return setconf_clock_period_us(gd);

	case GD_CCW_CMD_SET_OUT_PINS:
                return setconf_out_pins(gd);

	case GD_CCW_CMD_SET_IN_PINS:
                return setconf_in_pins(gd);

	case GD_CCW_CMD_SET_FILTER:
                return setconf_filter(gd);

	case GD_CCW_CMD_SET_IRQ_CONFIG:
                return setconf_irq_config(gd);
        
        default:
                panic("invalid ccwcmd in do_gd_setconf");
        }

        // NOTREACHED
}

static void gd_setconf(pch_cu_t *cu, pch_devib_t *devib) {
        pch_dev_call_devib_or_reject_then(cu, devib,
                do_gd_setconf, gd_start_cbindex);
}

static bool read_in_pins_rt_callback(repeating_timer_t *rt) {
        pch_unit_addr_t ua = (pch_unit_addr_t)(uint32_t)rt->user_data;
        gpio_dev_t *gd = get_gpio_dev(ua);
        gd->values.data[gd->values.offset++] = gd_read_in_pins(gd);
        uint16_t count = gd->values.count;
        if (gd->values.offset < count)
                return true; // continue with repeating timer

        pch_dev_send_final_then(&gd_cu, ua, gd->values.data, count,
                gd_start_cbindex);
        return false; // stop repeating timer
}

static int do_ccw_read(pch_cu_t *cu, pch_devib_t *devib, pch_unit_addr_t ua, gpio_dev_t *gd) {
        uint16_t n = devib->size;
        if (n == 0)
                return -EDATALENZERO;

        gd_init_in_pins(gd);

        gd->values.data[0] = gd_read_in_pins(gd);
        if (n == 1) {
                pch_dev_send_final_then(cu, ua, gd->values.data, n,
                        gd_start_cbindex);
                return 0;
        }

        gd->values.count = n;
        gd->values.offset = 1;
        gd_add_repeating_timer(gd, read_in_pins_rt_callback, ua);
        return 0;
}

static int do_ccw_write(pch_cu_t *cu, pch_devib_t *devib, pch_unit_addr_t ua, gpio_dev_t *gd) {
        uint16_t n = devib->size;
        if (n == 0)
                return -EDATALENZERO;

        if (n > VALUES_BUF_SIZE)
                n = VALUES_BUF_SIZE;

        pch_dev_receive_then(cu, ua, &gd->values.data, n,
                gd_write_cbindex);

        return 0;
}

static bool write_out_pins_rt_callback(repeating_timer_t *rt) {
        pch_unit_addr_t ua = (pch_unit_addr_t)(uint32_t)rt->user_data;
        gpio_dev_t *gd = get_gpio_dev(ua);
        uint8_t val = gd->values.data[gd->values.offset++];
        gd_write_out_pins(gd, val);

        uint16_t count = gd->values.count;
        if (gd->values.offset < count)
                return true; // continue with repeating timer

        pch_dev_update_status_ok(&gd_cu, ua);
        return false; // stop repeating timer
}

// do_gd_write is called from the gd_write callback from the CU after
// a do_ccw_write has received the values data from the channel
// and written it to gd->values.data.
static int do_gd_write(pch_cu_t *cu, pch_unit_addr_t ua) {
        gpio_dev_t *gd = get_gpio_dev(ua);
        if (!gd)
                return -EINVALIDDEV;

        pch_devib_t *devib = pch_get_devib(cu, ua);
        uint16_t n = devib->size;
        if (n == 0)
                return -EDATALENZERO;

        gd_init_out_pins(gd);

        uint8_t val = gd->values.data[0];
        gd_write_out_pins(gd, val);
        
        if (n == 1) {
                pch_dev_update_status_ok(&gd_cu, ua);
                return 0;
        }

        gd->values.count = n;
        gd->values.offset = 1;
        gd_add_repeating_timer(gd, write_out_pins_rt_callback, ua);
        return 0;
}

static void gd_write(pch_cu_t *cu, pch_devib_t *devib) {
        pch_dev_call_devib_or_reject_then(cu, devib,
                do_gd_write, gd_start_cbindex);
}

static inline bool filter_match(gd_filter_t filter, uint8_t val) {
        return (val & filter.mask) == filter.target;
}

static void complete_test(pch_cu_t *cu, pch_unit_addr_t ua, gpio_dev_t *gd) {
        uint8_t val = gd->values.data[0];
        uint8_t devs = PCH_DEVS_CHANNEL_END | PCH_DEVS_DEVICE_END;
        if (filter_match(gd->cfg.filter, val))
                devs |= PCH_DEVS_STATUS_MODIFIER;

        pch_dev_update_status_then(cu, ua, devs, gd_start_cbindex);
}       

static int do_gd_complete_test(pch_cu_t *cu, pch_unit_addr_t ua) {
        gpio_dev_t *gd = get_gpio_dev(ua);
        if (!gd)
                return -EINVALIDDEV;

        complete_test(cu, ua, gd);
        return 0;
}
                
static void gd_complete_test(pch_cu_t *cu, pch_devib_t *devib) {
        pch_dev_call_devib_or_reject_then(cu, devib,
                do_gd_complete_test, gd_start_cbindex);
}

static int do_ccw_test(pch_cu_t *cu, pch_devib_t *devib, pch_unit_addr_t ua, gpio_dev_t *gd) {
        gd->values.data[0] = gd_read_in_pins(gd);

        if (!devib->size) {
                complete_test(cu, ua, gd);
                return 0;
        }

        return pch_dev_send_norespond_then(cu, ua,
                &gd->values.data[0], 1, gd_complete_test_cbindex);
}

// do_gd_start is called from the devib's gd_start callback from the CU.
static int do_gd_start(pch_cu_t *cu, pch_unit_addr_t ua) {
        gpio_dev_t *gd = get_gpio_dev(ua);
        if (!gd)
                return -EINVALIDDEV;

        pch_devib_t *devib = pch_get_devib(cu, ua);
        uint8_t ccwcmd = devib->payload.p0;
        switch (ccwcmd) {
        case PCH_CCW_CMD_READ:
                return do_ccw_read(cu, devib, ua, gd);

        case PCH_CCW_CMD_WRITE:
                return do_ccw_write(cu, devib, ua, gd);

        case GD_CCW_CMD_TEST:
                return do_ccw_test(cu, devib, ua, gd);

        case GD_CCW_CMD_SET_CLOCK_PERIOD_US:
                return do_ccw_set_config(cu, ua, devib->size, gd,
                        ccwcmd, sizeof gd->cfg.clock_period_us);

	case GD_CCW_CMD_SET_OUT_PINS:
                return do_ccw_set_config(cu, ua, devib->size, gd,
                        ccwcmd, sizeof gd->cfg.out_pins);

	case GD_CCW_CMD_SET_IN_PINS:
                return do_ccw_set_config(cu, ua, devib->size, gd,
                        ccwcmd, sizeof gd->cfg.in_pins);

	case GD_CCW_CMD_SET_FILTER:
                return do_ccw_set_config(cu, ua, devib->size, gd,
                        ccwcmd, sizeof gd->cfg.filter);

	case GD_CCW_CMD_SET_IRQ_CONFIG:
                return do_ccw_set_config(cu, ua, devib->size, gd,
                        ccwcmd, sizeof gd->cfg.irq);

        case GD_CCW_CMD_GET_CLOCK_PERIOD_US:
                return do_ccw_get_config(cu, ua, devib->size,
                        &gd->cfg.clock_period_us,
                        sizeof gd->cfg.clock_period_us);

	case GD_CCW_CMD_GET_OUT_PINS:
                return do_ccw_get_config(cu, ua, devib->size,
                        &gd->cfg.out_pins, sizeof gd->cfg.out_pins);

	case GD_CCW_CMD_GET_IN_PINS:
                return do_ccw_get_config(cu, ua, devib->size,
                        &gd->cfg.in_pins, sizeof gd->cfg.in_pins);

	case GD_CCW_CMD_GET_FILTER:
                return do_ccw_get_config(cu, ua, devib->size,
                        &gd->cfg.filter, sizeof gd->cfg.filter);

	case GD_CCW_CMD_GET_IRQ_CONFIG:
                return do_ccw_get_config(cu, ua, devib->size,
                        &gd->cfg.irq, sizeof gd->cfg.irq);

        default:
                return -EINVALIDCMD;
        }

        // NOTREACHED
}

static void gd_start(pch_cu_t *cu, pch_devib_t *devib) {
        assert(proto_chop_cmd(devib->op) == PROTO_CHOP_START);
        pch_dev_call_devib_or_reject_then(cu, devib, do_gd_start,
                gd_start_cbindex);
}

void gd_cu_init(pch_cunum_t cunum, uint8_t dmairqix) {
        assert(!gd_cu_done_init);

        pch_cus_cu_init(&gd_cu, cunum, dmairqix, NUM_GPIO_DEVS);

        memset(gpio_devs, 0, sizeof(gpio_devs));
        gd_start_cbindex =
                pch_register_unused_devib_callback(gd_start);
        gd_setconf_cbindex =
                pch_register_unused_devib_callback(gd_setconf);
        gd_write_cbindex =
                pch_register_unused_devib_callback(gd_write);
        gd_complete_test_cbindex =
                pch_register_unused_devib_callback(gd_complete_test);

        gd_alarm_pool = alarm_pool_create_with_unused_hardware_alarm(NUM_GPIO_DEVS);
        gd_cu_done_init = true;
}

void gd_dev_init(pch_cu_t *cu, pch_unit_addr_t ua) {
        gpio_dev_t *gd = get_gpio_dev(ua);
        assert(gd);

        reset_gpio_dev(gd, ua);

        pch_dev_set_callback(cu, ua, gd_start_cbindex);
}
