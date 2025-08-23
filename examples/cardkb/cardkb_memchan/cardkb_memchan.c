#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/stdio.h"

#include "picochan/css.h"
#include "picochan/cu.h"

#include "../cardkb_api.h"

#define NUM_CARDKB_DEVS 1
#define FIRST_UA 0

#define CARDKB_ENABLE_TRACE true

const pch_cuaddr_t CUADDR = 0;
const pch_chpid_t CHPID = 0;

#define CARDKB_ENABLE_TRACE true

/*
 * cardkb_memchan runs the complete cardkb_dev Picochan example on a
 * single Pico. The CSS is run on core 0 and the CU on core 1.
 * Instead of needing physical channel connections between CSS
 * and CU, this configuration uses a memory channel (memchan)
 * so that CSS-to-CU communication happens directly via
 * memory-to-memory DMA for data transfers and 4-byte
 * writes/reads from memory for command transfers.
 */

static pch_cu_t cardkb_cu = PCH_CU_INIT(NUM_CARDKB_DEVS);

pch_dmaid_t css_to_cu_dmaid;
pch_dmaid_t cu_to_css_dmaid;
pch_dma_irq_index_t css_dmairqix = 0;
pch_dma_irq_index_t cu_dmairqix = 1;

extern void cardkb_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices);
extern void cardkb_i2c_init(i2c_inst_t **i2cp, uint8_t *addrp);
extern void cardkb_dev_init(pch_unit_addr_t ua, i2c_inst_t *i2c, uint8_t i2c_addr);

static void core1_thread(void) {
        pch_cus_init();
        pch_cus_set_trace(CARDKB_ENABLE_TRACE);
        pch_cus_configure_dma_irq_index_shared_default(cu_dmairqix);

        cardkb_cu_init(&cardkb_cu, FIRST_UA, NUM_CARDKB_DEVS);
        pch_cu_register(&cardkb_cu, CUADDR);
        pch_cus_trace_cu(CUADDR, CARDKB_ENABLE_TRACE);

        i2c_inst_t *i2c;
        uint8_t i2c_addr;
        cardkb_i2c_init(&i2c, &i2c_addr);
        cardkb_dev_init(FIRST_UA, i2c, i2c_addr);

        dmachan_tx_channel_t *txpeer = pch_chp_get_tx_channel(CHPID);
        pch_cus_memcu_configure(CUADDR, cu_to_css_dmaid,
                css_to_cu_dmaid, txpeer);

        pch_cu_start(CUADDR);

        while (1)
                __wfe();
}

static void light_led_for_three_seconds() {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

static char buff[64];

static cardkb_dev_config_t cdc = {
        .timeout_cs = 0xffff,   // never timeout
        .eol = '\r',            // end when Enter key pressed (yes, \r)...
        .minread = 0xff         // ...and not before
};

static pch_ccw_t configure_kb_prog[] = {
        { CARDKB_CCW_CMD_SET_CONFIG, 0, sizeof(cdc), (uint32_t)&cdc }
};

static pch_ccw_t read_line_from_kb_prog[] = {
        { PCH_CCW_CMD_READ, PCH_CCW_FLAG_SLI,
                sizeof(buff)-1, (uint32_t)&buff }
};

static void read_and_print_line() {
        pch_scsw_t scsw;

        puts("Type some keys on the CardKB, ending with Enter (<-')");
        pch_sch_run_wait(0, read_line_from_kb_prog, &scsw);
        // CCW count was sizeof(buff)-1
        uint16_t rescount = scsw.count;
        assert(rescount < sizeof(buff));
        uint n = sizeof(buff) - 1 - rescount; // 0...sizeof(buff)-1
        buff[n] = 0;
        printf("You typed: %s\n", buff);
}

int main(void) {
        bi_decl(bi_program_description("picochan cardkb test memchan CSS+CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();
        light_led_for_three_seconds();

        puts("Starting...");
        css_to_cu_dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        cu_to_css_dmaid = (pch_dmaid_t)dma_claim_unused_channel(true);
        css_dmairqix = 0;
        cu_dmairqix = 1;

        pch_memchan_init();

        pch_css_init();
        pch_css_set_trace(CARDKB_ENABLE_TRACE);
        pch_css_configure_dma_irq_index_shared(css_dmairqix,
                PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        pch_css_start(NULL, 0); // must set CSS dmairqix before this
        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_chp_alloc(chpid, 1); // Allocates SID 0
        pch_chp_set_trace(chpid, CARDKB_ENABLE_TRACE);

        multicore_launch_core1(core1_thread);
        sleep_ms(2000); // XXX not sure if there's a race

        dmachan_tx_channel_t *txpeer = pch_cu_get_tx_channel(CUADDR);
        pch_chp_configure_memchan(CHPID, css_to_cu_dmaid,
                cu_to_css_dmaid, txpeer);

        pch_sch_modify_enabled(0, true);
        pch_sch_modify_traced(0, CARDKB_ENABLE_TRACE);

        pch_chp_start(chpid);

        pch_scsw_t scsw;
        pch_sch_run_wait(0, configure_kb_prog, &scsw);

        while (1)
                read_and_print_line();
}
