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
#define NUM_CUS 2
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

bool pch_cus_set_trace(bool trace);

void pch_cus_init_dma_irq_handler(uint8_t dmairqix);

pch_cbindex_t pch_register_unused_devib_callback(pch_devib_callback_t cb);
```

### Initialisation of each CU

```
void pch_cus_cu_init(pch_cu_t *cu, pch_cunum_t cunum, uint8_t dmairqix, uint16_t num_devibs);

bool pch_cus_trace_cu(pch_cunum_t cunum, bool trace);

void pch_cus_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl);

void pch_cus_cu_start(pch_cunum_t cunum);
```

### Convenience API for device driver to its CU

#### Convenience API with fully general arguments

```
int pch_dev_set_callback(pch_cu_t *cu, pch_unit_addr_t ua, int cbindex_opt);
int pch_dev_send_then(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);
int pch_dev_send_zeroes_then(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);
int pch_dev_receive_then(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size, int cbindex_opt);
int pch_dev_update_status_advert_then(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, void *dstaddr, uint16_t size, int cbindex_opt);
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
int pch_dev_send(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, proto_chop_flags_t flags);
int pch_dev_send_final(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n);
int pch_dev_send_final_then(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_respond(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n);
int pch_dev_send_respond_then(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_norespond(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n);
int pch_dev_send_norespond_then(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, proto_chop_flags_t flags);
int pch_dev_send_zeroes_respond_then(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes_respond(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n);
int pch_dev_send_zeroes_norespond_then(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes_norespond(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n);
int pch_dev_receive(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size);
int pch_dev_update_status_then(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, int cbindex_opt);
int pch_dev_update_status(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs);
int pch_dev_update_status_advert(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, void *dstaddr, uint16_t size);
int pch_dev_update_status_ok_then(pch_cu_t *cu, pch_unit_addr_t ua, int cbindex_opt);
int pch_dev_update_status_ok(pch_cu_t *cu, pch_unit_addr_t ua);
int pch_dev_update_status_ok_advert(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size);
int pch_dev_update_status_error_advert_then(pch_cu_t *cu, pch_unit_addr_t ua, pch_dev_sense_t sense, void *dstaddr, uint16_t size, int cbindex_opt);
int pch_dev_update_status_error_then(pch_cu_t *cu, pch_unit_addr_t ua, pch_dev_sense_t sense, int cbindex_opt);
int pch_dev_update_status_error_advert(pch_cu_t *cu, pch_unit_addr_t ua, pch_dev_sense_t sense, void *dstaddr, uint16_t size);
int pch_dev_update_status_error(pch_cu_t *cu, pch_unit_addr_t ua, pch_dev_sense_t sense);
```

### Low-level API for device driver to its CU

The Convenience API functions above use this low-level API and are
more likely to be suitable instead of using these directly.

```
static inline void pch_devib_prepare_callback(pch_devib_t *devib, pch_cbindex_t cbindex);
static inline void pch_devib_prepare_count(pch_devib_t *devib, uint16_t count);
static inline void pch_devib_prepare_write_data(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags);
static inline void pch_devib_prepare_write_zeroes(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags);
static inline void pch_devib_prepare_read_data(pch_devib_t *devib, void *dstaddr, uint16_t size);
void pch_devib_prepare_update_status(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size);
void pch_devib_send_or_queue_command(pch_cu_t *cu, pch_unit_addr_t ua);
```
