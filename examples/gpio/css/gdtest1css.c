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

// CSS IRQ definitions
irq_num_t schib_func_irqnum = FIRST_USER_IRQ;
irq_num_t io_irqnum = FIRST_USER_IRQ + 1;

// io_callback is called from where CSS runs which is core 0 for us
void io_callback(pch_intcode_t ic, pch_scsw_t scsw) {
        // no-op	
        (void)ic;
        (void)scsw;
}

// We call init_css from core 0 so CSS runs there
void init_css(void) {
        pch_css_init();

        uint corenum = get_core_num();
        uint8_t dmairqix = (uint8_t)corenum;
        dprintf("Starting CSS: core %u, DMA IRQ index %u\n",
                corenum, dmairqix);

        pch_css_set_trace((bool)GD_ENABLE_TRACE);
        pch_css_start(dmairqix);

	// Set CSS to use IRQ schib_func_irqnum internally for when an
	// API call needs to trigger CSS to execute a function
        irq_set_exclusive_handler(schib_func_irqnum,
                pch_css_schib_func_irq_handler);
        irq_set_enabled(schib_func_irqnum, true);
        pch_css_set_func_irq(schib_func_irqnum);

	// Set CSS to raise IRQ IoIrqnum when a schib is added to notify list
        pch_css_set_io_irq(io_irqnum);

	// Make CSS invoke io_callback(sid, scsw) on each schib in
	// notify list when _callback_pending_schibs() is called
        pch_css_set_io_callback(io_callback);

	// Make CSS call callback_pending_schibs (and clear irq) when
	// io_irqnum is raised
        irq_set_exclusive_handler(io_irqnum, pch_css_io_irq_handler);
        irq_set_enabled(io_irqnum, true);

        // enable all ISCs (ignores non-existing ones)
        pch_css_set_isc_enable_mask(0xff);
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
	init_css();

        pch_chpid_t chpid = pch_chp_claim_unused(true);
        pch_sid_t first_sid = pch_chp_alloc(chpid, NUM_GPIO_DEVS);
        uart_inst_t *uart = prepare_uart_gpios();
        dprintf("Configuring CSS channel via UART%u\n", UART_NUM(uart));
        pch_chp_init_and_configure_uartchan(chpid, uart, BAUDRATE);
        pch_chp_set_trace(chpid, (bool)GD_ENABLE_TRACE);

        dprintf("Enabling subchannels %u through %u\n",
                first_sid, first_sid + NUM_GPIO_DEVS - 1);
        pch_sch_modify_enabled_range(first_sid, NUM_GPIO_DEVS, true);
        pch_sch_modify_traced_range(first_sid, NUM_GPIO_DEVS, true);

        dprintf("Starting channel %u\n", chpid);
        pch_chp_start(chpid);
        dprintf("CSS is ready\n");

        while (1)
                __wfe();
}
