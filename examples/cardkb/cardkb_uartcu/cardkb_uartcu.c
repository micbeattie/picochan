#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "pico/binary_info.h"

#include "picochan/cu.h"

#define NUM_CARDKB_DEVS 1
#define FIRST_UA 0
#define CUADDR 0

#define CARDKB_ENABLE_TRACE true

// Use uart1 via GPIO pins 4-7 for CU side
#define CU_UART uart1
#define CU_UART_TX_PIN 4
#define CU_UART_RX_PIN 5
#define CU_UART_CTS_PIN 6
#define CU_UART_RTS_PIN 7

// Baud rate for UART channel must match that used by CSS
#define CARDKB_BAUDRATE 115200

static pch_cu_t cardkb_cu = PCH_CU_INIT(NUM_CARDKB_DEVS);

static uart_inst_t *prepare_uart_gpios(void) {
        bi_decl(bi_4pins_with_func(CU_UART_RX_PIN,
                CU_UART_TX_PIN, CU_UART_RTS_PIN,
                CU_UART_CTS_PIN, GPIO_FUNC_UART));

        gpio_set_function(CU_UART_TX_PIN, GPIO_FUNC_UART);
        gpio_set_function(CU_UART_RX_PIN, GPIO_FUNC_UART);
        gpio_set_function(CU_UART_CTS_PIN, GPIO_FUNC_UART);
        gpio_set_function(CU_UART_RTS_PIN, GPIO_FUNC_UART);

        return CU_UART;
}

static void light_led_for_three_seconds(void) {
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(3000);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
}

extern void cardkb_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices);
extern void cardkb_i2c_init(i2c_inst_t **i2cp, uint8_t *addrp);
extern void cardkb_dev_init(pch_unit_addr_t ua, i2c_inst_t *i2c, uint8_t i2c_addr);

int main(void) {
        bi_decl(bi_program_description("picochan gpio_dev CU"));
        // work around timer stall during gdb debug with openocd:
        // https://github.com/raspberrypi/pico-feedback/issues/428
        timer_hw->dbgpause = 0;

        light_led_for_three_seconds();

        pch_cus_init();
        pch_cus_set_trace(CARDKB_ENABLE_TRACE);

        cardkb_cu_init(&cardkb_cu, FIRST_UA, 1);
        pch_cu_register(&cardkb_cu, CUADDR);
        pch_cus_trace_cu(CUADDR, CARDKB_ENABLE_TRACE);

        i2c_inst_t *i2c;
        uint8_t i2c_addr;
        cardkb_i2c_init(&i2c, &i2c_addr);
        cardkb_dev_init(FIRST_UA, i2c, i2c_addr);

        uart_inst_t *uart = prepare_uart_gpios();
        pch_cus_auto_configure_uartcu(CUADDR, uart, CARDKB_BAUDRATE);
        pch_cu_start(CUADDR);

        while (1)
                __wfe();
}
