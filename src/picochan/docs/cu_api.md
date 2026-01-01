## Control Unit (CU) API for Device Drivers Overview {#cu_api_page}

### Introduction

- "Device driver" software runs on core of CU and talks to actual devices
- "Device driver" is not a recognised term from the architectural
  view, and especially not from the application and channel subsystem
  side, but seems to be as good a term as any to use to refer to the
  software written to run CU-side to deal with the actual devices
- All device driver API calls are non-blocking (dozens to at most
  hundreds of cycles) and have no timing constraints
- API calls set some bits, update linked lists and cause the CU to
  send a single 4-byte operation packet down the channel to the CSS or,
  if already busy, queue it up so that CU will send it as soon as
  the current queue of operation commands have been sent
- At software init, register at least one callback function - there
  can be up to 239 per CU
- At device init time, `pch_dev_set_callback()` to set callback for "Start"
- When CSS fetches a CCW for the device with its
  `(command, flags, address, size)` fields, CSS sends Start request to
  CU which calls device's callback function
  * For a Read-type CCW, device uses `pch_dev_send...(...,srcaddr,size)`
    to send one or more chunks of data that (via CU->CSS) get written
    to the CCW data segments (CSS data-chains to following CCWs if needed)
  * For a Write-type CCW, device uses `pch_dev_receive...(...,dstaddr,size)`
    to request chunks of data from the CCW data segments
    (CSS data-chains to following CCWs if needed)
- Arguments to those API calls (or an explicit `pch_dev_set_callback`)
  can set the callback index to a different (already-registered) one
  to be used the next time the CU has reason to call the device driver
- Callbacks can happen
  * when a command has been sent (so CU is ready for another)
  * when a requested update is received from CSS about "how much room is
    left in the data segment"
  * or for (rare) "stop as soon as you can" requests (application
    "HALT SUBCHANNEL", `pch_sch_halt()`)
- When device has finished with that CCW command and its (data-chain of)
  1 or more CCW segments, it uses `pch_update_status...(...,devstatus)`
  to cause the CSS to finish that CCW command. The devstatus can be
  * "normal" (CSS either command-chains to next CCW or notifies final
    state to application)
  * include "error" flags (prevents command-chaining and gets notified
    to application)
  * or "normal with StatusModifier" (CSS skips a CCW to allow for
  conditional logic in the channel program decided by device side)
- Device driver should document (for the application API user to see)
  what CCW command codes it recognises and what the associated data
  of the CCW (if any) is used for
  * may well be simply be "CCW command code 1 is Write" (when "Write"
    has an obvious device-specific meaning) and/or "CCW command code 2
    is Read" (when "Read" has an obivous device-specific meaning).
  * More complex device drivers may go wild with many different
    recognised command codes and data segment formats.
  * Command codes available to device drivers are 1 to 239 (0xef)
    with even ones being Read-type (application reads from device)
    and odd ones being Write-type (application writes to device).

Since there are not yet enough code comment Doxygen annotations to
divide the generated documentation into topics properly, there
follows a summary of the main definitions (e.g. types, macros and
API functions) for use by CU-side code. They are described in the
Doxygen-generated documentation but some may be in a Topics
sub-section, some may be in "Data Structures" and some may be under
"Files".

Although the above covers the low-levels details of what the CU does
and how device drivers must behave, there is now a (somewhat) higher
level API for implementing device drivers: this is the "hldev"
("high-level device") API documented in topic picochan_hldev.
That should typically be the first API to consider when implementing
a device driver.

### Types

```
typedef struct pch_cu pch_cu_t;

typedef uint8_t pch_cbindex_t;

typedef struct pch_devib pch_devib_t;

typedef void (*pch_devib_callback_t)(pch_cu_t *cu, pch_devib_t *devib);

typedef struct pch_dev_sense pch_dev_sense_t;
```

### Compile-time constants and definitions - examples:

```
#define PCH_NUM_CUS 2
```

### Debugging assertions:

```
#define PARAM_ASSERTIONS_ENABLED_PCH_CUS 1
#define PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN 1
#define PARAM_ASSERTIONS_ENABLED_PCH_TXSM 1
```

### Initialisation of whole CU subsystem

```
void pch_cus_init(void);

// Each CU runs device driver callbacks in an async context of type
// async_context_threadsafe_background_config_t. Optionally, you can
// explicitly set one to be used when the first CU that needs one is
// configured, or else one will be created automatically.
async_context_t *pch_cus_configure_default_async_context(async_context_threadsafe_background_config_t *config);

bool pch_cus_set_trace(bool trace);

// If not using the hldev API, register your callbacks:
pch_cbindex_t pch_register_unused_devib_callback(pch_devib_callback_func_t cbfunc, void *cbctx);

// Optionally configure explicit IRQ index(es) and IRQ handler
// attributes for DMA IRQs (and PIO irqs where relevant) or leave
// to auto-configure with defaults:
void pch_cus_ignore_irq_index_t(pch_irq_index_t irq_index);
void pch_cus_configure_dma_irq(pch_irq_index_t irq_index, int order_priority);
void pch_cus_configure_dma_irq_exclusive(pch_irq_index_t irq_index);
void pch_cus_configure_dma_irq_shared(pch_irq_index_t irq_index, uint8_t order_priority);
void pch_cus_configure_dma_irq_shared_default(pch_irq_index_t irq_index);

void pch_cus_configure_pio_irq(PIO pio, pch_irq_index_t irq_index, int order_priority);
void pch_cus_configure_pio_irq_exclusive(PIO pio, pch_irq_index_t irq_index);
void pch_cus_configure_pio_irq_shared(PIO pio, pch_irq_index_t irq_index, uint8_t order_priority);
void pch_cus_configure_pio_irq_shared_default(PIO pio, pch_irq_index_t irq_index);

// If using any PIO channel CUs, configure each PIO instance that is
// going to be used by a channel:
#define MY_PIO pio0
pch_pio_config_t cfg = pch_pio_get_default_config(MY_PIO);
pch_piochan_init(&cfg);
```

### Initialisation of each CU

```
pch_cu_t foo_cu = PCH_CU_INIT(num_devibs);
// or, if num_devbs is not a compile-time constant, initialise at runtime with:
void pch_cu_init(pch_cu_t *cu, uint16_t num_devibs);

// register at a given control unit address:
pch_cu_register(pch_cu_t *cu, pch_cuaddr_t cua);

bool pch_cus_trace_cu(pch_cuaddr_t cua, bool trace);

// If CU connection is as a PIO channel...:
pch_piochan_pins_t pins = {
        .tx_clock_in = MY_TX_CLOCK_IN_PIN,
        .tx_data_out = MY_TX_DATA_OUT_PIN,
        .rx_clock_out = MY_RX_CLOCK_OUT_PIN,
        .rx_data_in = MY_RX_DATA_IN_PIN
};
pch_piochan_config_t pc = pch_piochan_get_default_config(pins);
void pch_cus_piocu_configure(pch_cuaddr_t cua, pch_pio_config_t *cfg, pch_piochan_config_t *pc);

// If CU connection is as a UART channel:
#define MY_UART uart0
gpio_set_function(MY_UART_TX_PIN, GPIO_FUNC_UART);
gpio_set_function(MY_UART_RX_PIN, GPIO_FUNC_UART);
gpio_set_function(MY_UART_CTS_PIN, GPIO_FUNC_UART);
gpio_set_function(MY_UART_RTS_PIN, GPIO_FUNC_UART);

pch_uartchan_config_t cfg = pch_uartchan_get_default_config(MY_UART);
void pch_cus_uartcu_configure(pch_cuaddr_t cua, uart_inst_t *uart, pch_uartchan_config_t *cfg);

// If CU connection is as a memory channel:
// For a memchan, no physical channel connection is needed
// but the CSS-side code and CU-side code must know about each
// other's id number (CHPID and CU address) to connect the sides.
pch_channel_t *chpeer = pch_chp_get_channel(CHPID);
pch_cus_memcu_configure(CUADDR, chpeer);

// Start CU. Returns immediately after setting all CU handling to
// happen via interrupt handlers and callbacks from those.
// So if your CU does not need to do anything other than serving
// up its devices, you can follow with an infinite "__wfe()" loop.
void pch_cu_start(pch_cuaddr_t cua);
```

### Convenience low-level API for device driver to its CU

In general, use the higher-level device API (hldev) instead of this
but may occasionally need the following:

#### Convenience API with fully general arguments

```
int pch_dev_set_callback(pch_devib_t *devib, int cbindex_opt);
int pch_dev_call_or_reject_then(pch_devib_t *devib, pch_dev_call_func_t f, int reject_cbindex_opt);
void pch_dev_call_final_then(pch_devib_t *devib, pch_dev_call_func_t f, int cbindex_opt);

int pch_dev_send_then(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);
int pch_dev_send_zeroes_then(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);
int pch_dev_receive_then(pch_devib_t *devib, void *dstaddr, uint16_t size, int cbindex_opt);
int pch_dev_update_status_advert_then(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size, int cbindex_opt);
```

#### Convenience API with some fixed arguments

* Omitting `_then` avoids setting devib callback by hardcoding -1
as the `cbindex_opt` argument of the full `_then` function.

* For `send` and `send_zeroes` family, the `flags` argument is set to
    * `PROTO_CHOP_FLAG_END` for the `_final` variant,
    * `PROTO_CHOP_FLAG_RESPONSE_REQUIRED` for the _respond variant
    * 0 for the `_norespond` variant

* For `pch_dev_update_status_ok` family, call the corresponding
`pch_dev_update_status_` function with `DeviceEnd|ChannelEnd`

* For `pch_dev_update_status_error` family, set `devib->sense` to the
`sense` argument then call the corresponding `pch_dev_update_status_`
function with a device status of `DeviceEnd|ChannelEnd|UnitCheck`

```
int pch_dev_send(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags);
int pch_dev_send_final(pch_devib_t *devib, void *srcaddr, uint16_t n);
int pch_dev_send_final_then(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_respond(pch_devib_t *devib, void *srcaddr, uint16_t n);
int pch_dev_send_respond_then(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_norespond(pch_devib_t *devib, void *srcaddr, uint16_t n);
int pch_dev_send_norespond_then(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags);
int pch_dev_send_zeroes_respond_then(pch_devib_t *devib, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes_respond(pch_devib_t *devib, uint16_t n);
int pch_dev_send_zeroes_norespond_then(pch_devib_t *devib, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes_norespond(pch_devib_t *devib, uint16_t n);
int pch_dev_receive(pch_devib_t *devib, void *dstaddr, uint16_t size);
int pch_dev_update_status_then(pch_devib_t *devib, uint8_t devs, int cbindex_opt);
int pch_dev_update_status(pch_devib_t *devib, uint8_t devs);
int pch_dev_update_status_advert(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size);
int pch_dev_update_status_ok_then(pch_devib_t *devib, int cbindex_opt);
int pch_dev_update_status_ok(pch_devib_t *devib);
int pch_dev_update_status_ok_advert(pch_devib_t *devib, void *dstaddr, uint16_t size);
int pch_dev_update_status_error_advert_then(pch_devib_t *devib, pch_dev_sense_t sense, void *dstaddr, uint16_t size, int cbindex_opt);
int pch_dev_update_status_error_then(pch_devib_t *devib, pch_dev_sense_t sense, int cbindex_opt);
int pch_dev_update_status_error_advert(pch_devib_t *devib, pch_dev_sense_t sense, void *dstaddr, uint16_t size);
int pch_dev_update_status_error(pch_devib_t *devib, pch_dev_sense_t sense);
```

### Low-level API for device driver to its CU

Even when the higher-level device API (hldev) is not appropriate,
the Convenience API functions above are more likely to be suitable
instead of using these directly.

```
static inline void pch_devib_prepare_callback(pch_devib_t *devib, pch_cbindex_t cbindex);
static inline void pch_devib_prepare_count(pch_devib_t *devib, uint16_t count);
static inline void pch_devib_prepare_write_data(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags);
static inline void pch_devib_prepare_write_zeroes(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags);
static inline void pch_devib_prepare_read_data(pch_devib_t *devib, void *dstaddr, uint16_t size);
void pch_devib_prepare_update_status(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size);
void pch_devib_send_or_queue_command(pch_devib_t *devib);
```
