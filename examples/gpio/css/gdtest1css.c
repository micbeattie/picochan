#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/binary_info.h"

#include "picochan/css.h"

#include "../gd_debug.h"
#include "../gd_channel.h" 

unsigned char d[8192] __aligned(1024); // we'll put test data in here

#define NUM_GPIO_DEVS      8

// Use uart0 via GPIO pins 0-3 for CSS side
#define GDTEST1_UART_NUM 0
#define GDTEST1_UART_TX_PIN 0
#define GDTEST1_UART_RX_PIN 1
#define GDTEST1_UART_CTS_PIN 2
#define GDTEST1_UART_RTS_PIN 3

static uart_inst_t *prepare_uart_gpios(void) {
        bi_decl_if_func_used(bi_4pins_with_func(GDTEST1_UART_RX_PIN,
                GDTEST1_UART_TX_PIN, GDTEST1_UART_RTS_PIN,
                GDTEST1_UART_CTS_PIN, GPIO_FUNC_UART));
        gpio_set_function(GDTEST1_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDTEST1_UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDTEST1_UART_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(GDTEST1_UART_RTS_PIN, GPIO_FUNC_UART);

        return UART_INSTANCE(GDTEST1_UART_NUM);
}

void io_callback(pch_intcode_t ic, pch_scsw_t scsw) {
        dprintf("io_callback for SID:%04X with IntParm:%08lx and SCSW:\n",
                ic.sid, ic.intparm);
        dprintf("  next_CCW_address:%08lx dev_status:%02x sch_status:%02x residual_count=%d\n",
                scsw.ccw_addr, scsw.devs, scsw.schs, scsw.count);
}

// Quick and dirty way to force variables and functions to be
// instantiated as visible in the debugger even though
// apparently unused.
void *volatile discard;
static void use(void *p) {
        discard = p;
}

void force_runtime_access_to_functions_and_data(void) {
        // CSS API
        use(pch_sch_start);
        use(pch_sch_resume);
        use(pch_sch_test);
        use(pch_sch_modify);
        use(pch_sch_store);
        use(pch_sch_store_pmcw);
        use(pch_sch_store_scsw);
        use(pch_sch_cancel);
        use(pch_test_pending_interruption);
        use(pch_css_set_isc_enabled);
        // dmachan functions for debugging
        use(dmachan_handle_tx_irq);
        use(dmachan_handle_rx_irq);

        // Extra CSS convenience API
        use(pch_sch_modify_intparm);
        use(pch_sch_modify_flags);
        use(pch_sch_modify_isc);
        use(pch_sch_modify_enabled);
        use(pch_sch_modify_traced);
        use(pch_sch_wait);
        use(pch_sch_wait_timeout);
        use(pch_sch_run_wait);
        use(pch_sch_run_wait_timeout);

        // ...and even some functions/data in this file
        use(d);
}

static void light_led_for_three_seconds() {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

int main(void) {
        bi_decl(bi_program_description("picochan gpio_dev test1 UART0 CSS"));
        force_runtime_access_to_functions_and_data();

        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        stdio_init_all();
        light_led_for_three_seconds();

        dprintf("Initialising CSS\n");
        pch_css_init();
        pch_css_set_trace((bool)GD_ENABLE_TRACE);
        pch_css_start(io_callback, 0xff);

        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_sid_t first_sid = pch_chp_alloc(chpid, NUM_GPIO_DEVS);

        uart_inst_t *uart = prepare_uart_gpios();
        dprintf("Configuring CSS channel CHPID=%u via UART%u\n",
                chpid, UART_NUM(uart));
        pch_chp_auto_configure_uartchan(chpid, uart, BAUDRATE);
        pch_chp_set_trace(chpid, (bool)GD_ENABLE_TRACE);

        dprintf("Enabling subchannels %u through %u\n",
                first_sid, first_sid + NUM_GPIO_DEVS - 1);
        pch_sch_modify_enabled_range(first_sid, NUM_GPIO_DEVS, true);
        pch_sch_modify_traced_range(first_sid, NUM_GPIO_DEVS, true);

        dprintf("Starting channel CHPID=%u\n", chpid);
        pch_chp_start(chpid);
        dprintf("CSS is ready\n");

        while (1)
                __wfe();
}
