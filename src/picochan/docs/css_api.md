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

// Optionally use pch_css_auto_configure_..., pch_css_configure_...
// or pch_css_set functions to choose, configure and enable IRQ
// handlers for DMA, function and I/O IRQs, then auto-configure
// and enable any remaining ones with:

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
// return the SID of the first.
pch_sid_t pch_chp_alloc(pch_chpid_t chpid, uint16_t num_devices);
```

#### Initialise a channel to a UART CU

```
// Initialise and configure a UART channel with default parameters...
void pch_chp_auto_configure_uartchan(pch_chpid_t chpid, uart_inst_t *uart, dma_channel_config ctrl);
// ...or, less commonly, configure with non-default DMA control
// register flags after initialising the UART beforehand
void pch_chp_configure_uartchan(pch_chpid_t chpid, uart_inst_t *uart, dma_channel_config ctrl);
```

#### Initialise a channel to a memchan (cross-core) CU

```
void pch_memchan_init();

dmachan_tx_channel_t *pch_cu_get_tx_channel(pch_chpid_t chpid);

void pch_chp_configure_memchan(pch_chpid_t chpid, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer);
```

#### Initialise a channel to a pio CU

TBD

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
