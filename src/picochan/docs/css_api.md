## Channel Subsystem (CSS) API for Applications {#css_api_page}

### Introduction

- Application API for doing I/O just uses CSS
- The CSS represents each device it knows about as a
  _subchannel_ and the application API interacts with a subchannel
  by using its 16-bit _Subchannel ID_ (SID)
- The SID is an index into a CSS-managed global array of
  control blocks called _Subchannel Information Blocks_ (SCHIBS)
- API is to start and manage channel programs of
  [Channel Command Words (CCWs)](@ref channel_program_page)
- `pch_sch_start(sid, addr)` to start a channel program from CCW address `addr`
- channel program runs async in CSS by talking over the channel
  to the CU which talks to device
- notification from CSS by irq or callback when
  * channel program complete
  * or at marked CCWs to notify partial progress
  * which can "just notify" or suspend then resume with `pch_sch_resume(sid)`

Since there are not yet enough code comment Doxygen annotations to
divide the generated documentation into topics properly, there
follows a summary of the main definitions (e.g. types, macros and
API functions) for use by CSS-side code. They are described in the
Doxygen-generated documentation but some may be in a Topics
sub-section, some may be in "Data Structures" and some may be under
"Files".

### Compile-time constants and definitions - examples:

```
#define PCH_NUM_CHANNELS 4
#define PCH_NUM_SCHIBS 40
```

### Debugging assertions:

```
#define PARAM_ASSERTIONS_ENABLED_PCH_CSS 1
#define PARAM_ASSERTIONS_ENABLED_PCH_TXSM 1
```

### Types

```
typedef struct pch_schib pch_schib_t;

typedef struct pch_pmcw pch_pmcw_t;

typedef struct pch_intcode pch_intcode_t;

typedef void(*io_callback_t)(pch_intcode_t, pch_scsw_t);
```

### Initialisation of whole CSS

```
void pch_css_init(void);

bool pch_css_set_trace(bool trace);

// Optionally override the defaults for IRQ index and IRQ handler
// attributes with pch_css_set_irq_index(),
// pch_css_configure_dma_irq_...(), pch_css_configure_pio_irq_...(),
// pch_css_configure_func_irq_...(), pch_css_configure_io_irq...().
// Otherwise, they will be configured automatically when needed
// using defaults.

void pch_css_start(io_callback_t io_callback);

void pch_css_set_isc_enable_mask(uint8_t mask);
```

### Allocation of subchannels in a channel to a CU

```
// Claim an unused channel (returns its pch_chpid_t or
// returns -1 or panics on failure)...
int pch_chp_claim_unused(bool required);
// ...or (less commonly) claim a specific chpid
// (panics on failure)
void pch_chp_claim(pch_chpid_t chpid);

// Allocate num_devices consecutive subchannels on the channel and
// return the SID of the first. The first SID will reference the
// device with unit address 0 on the Control Unit that the channel
// is connected to. Subsequent SIDs reference unit addresses
// 1, 2, 3, ..., num_devices-1.
pch_sid_t pch_chp_alloc(pch_chpid_t chpid, uint16_t num_devices);
```

#### Initialise a channel to a PIO CU

```
// Before creating any PIO channels, initialise any PIO instance
// that will be used. This loads the piochan PIO programs into the
// instance.
#define MY_PIO pio0
pch_pio_config_t cfg = pch_pio_get_default_config(MY_PIO);
// Optionally change fields of cfg to use non-default irq_index,
// IRQ handler attributes (exclusive/shared/priority) or if you need
// to load the PIO programs manually at explicitly chosen offsets.
pch_piochan_init(&cfg);
 
// Initialise and configure a PIO channel. Each PIO instance can
// support two separate channels. Each channel needs 4 GPIO pins to
// be connected to its peer Control Unit: tx_clock_in, tx_data_out,
// rx_clock_out, rx_data_in and the pins need to be connected with
// tx_clock_in<->rx_clock_out, tx_data_out<->rx_data_in. Any 4 pins
// can be chosen within the block of 32 pins addressable by the chosen
// PIO instance - they do not need to be consecutive.
pch_piochan_pins_t pins = {
        .tx_clock_in = BLINK_TX_CLOCK_IN_PIN,
        .tx_data_out = BLINK_TX_DATA_OUT_PIN,
        .rx_clock_out = BLINK_RX_CLOCK_OUT_PIN,
        .rx_data_in = BLINK_RX_DATA_IN_PIN
};
pch_piochan_config_t pc = pch_piochan_get_default_config(pins);
// Optionally set explicit state machine numbers in pc (tx_sm and
// rx_sm) or leave them at their default of -1 for unused state
// machines to be claimed automatically.
pch_chp_configure_piochan(chpid, &cfg, &pc);
```

#### Initialise a channel to a UART CU

```
// For the UART peripheral instance whose 4 GPIO pins you have
// connected to the peer Control Unit, select the UART function
// of the GPIOs so the UART can drive them. As usual for UART to
// UART connections, connect TX<->RX and CTS<->RTS. Note that
// all 4 pins *must* be connected to the CU - hardware flow control
// is mandatory.
gpio_set_function(MY_UART_TX_PIN, GPIO_FUNC_UART);
gpio_set_function(MY_UART_RX_PIN, GPIO_FUNC_UART);
gpio_set_function(MY_UART_CTS_PIN, GPIO_FUNC_UART);
gpio_set_function(MY_UART_RTS_PIN, GPIO_FUNC_UART);

// Initialise the channel using your chosen baud rate which must
// match the baud rate on the CU side.
#define MY_UART uart0
#define MY_BAUDRATE 115200
pch_uartchan_config_t cfg = pch_uartchan_get_default_config(uart);
cfg.baudrate = MY_BAUDRATE;
pch_chp_configure_uartchan(chpid, MY_UART, &cfg);
```

#### Initialise a memchan channel to a (cross-core) CU

```
void pch_memchan_init();

// For a memchan, no physical channel connection is needed
// but the CSS-side code and CU-side code must know about each
// other's id number (CHPID and CU address) to connect the sides.
pch_channel_t *chpeer = pch_cu_get_channel(CUADDR);
pch_chp_configure_memchan(CHPID, chpeer);

```

### Start a channel to a CU

```
bool pch_chp_set_trace(pch_chpid_t chpid, bool trace);

void pch_chp_start(pch_chpid_t chpid);
```

### Set PMCW flags of a subchannel to enable/disable, trace or change ISC

```
int pch_sch_modify_flags(pch_sid_t sid, uint16_t flags);
```

### Start, monitor and control channel programs for a subchannel

```
int pch_sch_start(pch_sid_t sid, pch_ccw_t *ccw_addr);
int pch_sch_resume(pch_sid_t sid);
int pch_sch_test(pch_sid_t sid, pch_scsw_t *scsw);
int pch_sch_modify(pch_sid_t sid, pch_pmcw_t *pmcw);
int pch_sch_store(pch_sid_t sid, pch_schib_t *out_schib);
// int pch_sch_halt(pch_sid_t sid);
// int pch_sch_cancel(pch_sid_t sid);
pch_intcode_t pch_test_pending_interruption(void);
```

### Variations and wrappers for convenience or optimisation

```
int pch_sch_wait(pch_sid_t sid, pch_scsw_t *scsw);
int pch_sch_wait_timeout(pch_sid_t sid, pch_scsw_t *scsw, absolute_time_t timeout_timestamp);
int pch_sch_run_wait(pch_sid_t sid, pch_ccw_t *ccw_addr, pch_scsw_t *scsw);
int pch_sch_run_wait_timeout(pch_sid_t sid, pch_ccw_t *ccw_addr, pch_scsw_t *scsw, absolute_time_t timeout_timestamp);

int pch_sch_store_pmcw(pch_sid_t sid, pch_pmcw_t *out_pmcw);
int pch_sch_store_scsw(pch_sid_t sid, pch_scsw_t *out_scsw);

int pch_sch_modify_intparm(pch_sid_t sid, uint32_t intparm);
int pch_sch_modify_flags(pch_sid_t sid, uint16_t flags);
int pch_sch_modify_isc(pch_sid_t sid, uint8_t isc);
int pch_sch_modify_enabled(pch_sid_t sid, bool enabled);
int pch_sch_modify_traced(pch_sid_t sid, bool traced);
```
