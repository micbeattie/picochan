# Picochan - a channel subsystem for Raspberry Pi Pico

## Introduction

Picochan is
* library software that runs on Raspberry Pi Pico or, more generally,
  RP2040, RP2350 and family chips
* inspired by the I/O architecture of IBM mainframes which provides
  application-facing I/O machine instructions that trigger the
  "Channel Subsystem" (CSS) to run asynchronous channel programs of
  "Channel Command Words" communicated to a remote Control Unit (CU)
  that performs the low-level device I/O
* implements a CSS and CU(s) as low-level libraries to drive available
  Pico peripherals (e.g. DMA, UART, PIO) that allow separating the
  CSS from CU on separate cores of one Pico or separate Picos
* licensed as Open Source
* not designed particularly to be a replacement or "better than" any
  existing way of writing software for Pico that does I/O, whether
  with plain software libraries or addition peripherals (e.g. USB)
  or hardware (e.g. breakout boards)
* written for interest in order to find out whether this I/O model
  which proved useful for mainframes and their programmers when
  introduced 60+ years ago is a useful model now that Pico
  microcontroller I/O capabilities have caught up with those of
  mainframes 30-40 years ago

Picochan is *NOT*
* compatible in any way with actual mainframe I/O software,
  hardware, channels, CUs or I/O or device hardware
* anything that does or would make sense to port to or
  compile on actual mainframe hardware, whether old or new. This is
  fundamental since Picochan is written to use the low-level
  microcontroller-style "bit-banging" of DMA and pin-based
  peripherals (UART, PIO) in order to implement its functionality -
  the functionality which actual IBM mainframe architectures
  (from S/360 right up to modern z/Architecture) hide invisibly
  behind their actual hardware/firmware-implemented I/O architecture.

## Design

- CSS (Channel Subsystem) runs on one core. One CSS only.
- Application API calls functions from this core
  - All application API calls are short and non-blocking (dozens of
    cycles), just set some bits, update linked lists and raise an IRQ
  - CSS runs from IRQ handlers (short, non-blocking - dozens of cycles,
    prod Pico peripherals (e.g. UART, PIO, DMA), no timing constraints

- Each Control Unit (CU) runs on its own core (same or different Pico)
- Can be just one CU or up to 256
- Each has a "Control Unit number" (CU number), 0-255
- Each CU can address up to 256 devices
- Each device on a CU has a "unit address" 0-255
- Connection between CU and its CSS is via a *channel*
- Currently implemented channels are:
  * uart channel ("uartchan")
    - uses one Pico UART on CSS and one on CU side
    - hardware connections: TX, RX, RTS, CTS, GND
    - Notee: RTS and CTS are absolutely required
  * memory channel ("memchan")
    - between two cores on same Pico: one core runs CSS; one core runs CU
    - no hardware connections needed
  * pio channel ("piochan")
    - uses PIO to drive the CSS<->CU protocol
    - hardware connections: TX, RX, CLK, RTS, CTS, GND
    - custom PIO state machine programs - all connections absolutely required

## CSS <-> CU protocol

- CSS<->CU protocol is custom for Picochan - none of the CSS <-> CU
  connectivity and protocol options used for actual mainframes in
  the past or present (parallel channels, ESCON, FICON) is suitable
  for consideration for use with a microcontroller 
- 4-byte operation commands; 16-bit data segment counts
- All channel types use DMA for data segment transfer to/from channel
- Channels are (for pio and uart channels) hardware FIFOs direct
to/from Pico peripherals or (for mem channel) a single
cross-memory 32-bit load/store with cross-memory DMA for data segments
- CSS represents each device to the application as a "subchannel"
  * A subchannel is represented in the CSS as a "Subchannel Information
    Block" (SCHIB), `pch_schib_t` (a 32-byte structure)
  * The CSS has a global array of SCHIBs (fixed size chosen at
    compile-time), addressed by a "Subsystem identification word" (SID)
  * SID is a 16-bit integer (`pch_sid_t` typdef for uint16_t) which
  * The schib has fields for the device's control unit number and unit
    address for the CSS to use to contact the device's CU and identify
    a chosen device to that CU

## Application API

- Application API for doing I/O just uses CSS
- Application API uses the 16-bit SID to address a device
- API is to start and manage channel programs of CCWs (q.v.)
- `pch_sch_start(sid, addr)` to start a channel program from CCW address addr
- channel program runs async in CSS by talking to CU which talks to device
- notification from CSS by irq or callback when
  * channel program complete
  * or at marked CCWs to notify partial progress
  * which can "just notify" or suspend then resume with `pch_sch_resume(sid)`

## Device driver API

- "Device driver" software run on core of CU and talk to actual devices
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
- At software init, register at least one callback functions
  (up to 239 per CU)
- At device init time, `pch_dev_set_callback()` to set callback for "Start"
- When CSS fetches a CCW for the device with its
  (command, flags, address, size) fields, CSS sends Start request to
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

## Channel programs

### Definition

A channel program is a series of (and often just one) 8-byte
  Channel Command Word (CCW)

A CCW has API C type `pch_ccw_t` which is architecturally defined:
  * 8-bit command code
  * 8-bit flags
  * 16-bit data segment size
  * 32-bit data address

The required alignment for a CCW is 4-bytes, not 8-bytes, i.e.
a CCW must be at an address divisble by 4 but need not be at
an address that is divisible by 8.

Each subchannel can be running a channel program independently,
started (asynchronously) by the application API
`pch_sch_start(sid, ccwaddr)` which starts a channel program
sending CCW commands to the device addressed by SID `sid`
(via its CU) starting with the CCW at address `ccwaddr`

When the CSS executes a CCW, it sends the device (via communication
over the channel with the CU that owns the device) a request with
the given command code in the CCW. That can simply be a "Write"
(command code 1) or "Read" (command code 2) - where Write and Read
mean whatever the device driver chooses them to mean - or a
different device-driver-documented command code (up to 239) for
more complex commands.
* All odd command codes are Write-type (the CCW address and size
  provides a data segment for the device to receive and use to
  "write to the device")
* All even command codes are Read-type (the CCW address and size
  provides a data segment to be written to by the device).

Since the device driver is running on remotely on the CU
  (whether an entirely differnt Pico or the other core of the
  same Pico that the CSS runs on) the data transfers from/to the
  CCW data segment to/from the device happen via the channel and
  Pico peripherals (DMA, UART, PIO whatever), as driven by the
  CSS/CU software. _This is the the whole point of this software_.

The data area for the device to read/write begins with the
`(address, count)` segment in that first CCW but can continue...
* if the "chain-data" flag (`PCH_CCW_FLAG_CD`) is set in the CCW flags
  field, then when the device exhausts that segment, the CSS fetches
  the next CCW in memory (next 8 bytes) and the device continues its
  reading/writing from/to the `(address,count)` in the new CCW.
  For that new ("data-chained") CCW, the command field is ignored.

- When the data area is exhausted from that CCW command (the segment
  data-chained CCWs), the channel program will finish...unless the
  "chain-command" flag is set in the most recent CCW. See further down
  for what happens for command chaining.

When the channel program finishes, it is because the device has
sent a channel operation to the CSS saying "UpdateStatus"
(a CU->CSS operation code) with an 8-bit device status whose
flags say "finish the channel program".

The "device status" is an 8-bit architected set of flags for the
device to inform the CSS and the application
* The device can inform the CSS at any time about its status - it
  arrives at the CSS from the CU in an "UpdateStatus" protocol
  operation
* When a device has finished processing a CCW command, it will
  (and indeed must) send an UpdateStatus with a device status whose
  flags indicate that completion
* A device can send an UpdateStatus even when it is not in the
  process of dealing with a channel program - this is known as an
  "unsolicited" device status and it includes an "Alert" flag so
  that an application can be notified asynchronously about an
  event of interest at a device even when no channel program is in
  progress for that subchannel.
* The CSS makes that 8-bit device status visible to the application
  by storing it in the devs field in the Subchannel Status Word
  ("SCSW") part of the SCHIB at times in time for when the
  subchannel is notified. (Note that the fields of the SCSW,
  including that for the device status, may not contain meaningful
  values at other times.)

### When a CCW command in a channel program is finished

When the device has finished with one CCW command (including any
following data-chained CCWs) it sends an UpdateStatus with a
device status whose flags indicate that it has finished that CCW
command (flags including DeviceEnd and ChannelEnd)

At this point the CSS looks at the status of the subchannel and
the flags field of the CCW to decide how to continue, testing the
following conditions in order:
* If the subchannel has any unusual state (for example a CSS-side
  error or the application has done a `pch_sch_halt(sid)`) then
  then the channel program ends - see the "Channel program ending"
  section below.
* If the CCW "chain-command" flag (`PCH_CCW_FLAG_CC`) is _not_ set,
  then the channel program ends
* If the device status has flags that indicate any "unusual conditions"
  (anything other than a simple DeviceEnd|ChannelEnd and an optional
  StatusModifier) then the channel program ends
* Here, the subchannel is OK, the device status indicates the CCW
  command was processed with no unusual conditions and the CCW
  CCW "chain-command" flag is present: the CSS proceeds with
  "command chaining" as described immediately below.

### Channel program command chaining

When an individual CCW command has finished and command-chaining is
appropriate (see above for when that is), rather than ending the
channel program, the CSS proceeds by fetching another CCW.
* If the device status did _not_ include the StatusModifier flag,
  then the next CCW fetched is the one in memory immediately after
  the previous CCW (i.e. at an address 8 bytes beyond the previous
  CCW address)
* If the device status _did_ include the StatusModifier flag,
  then the CCW "skips" the next CCW and fetches the one after that
  (i.e. at an address 16 bytes beyond the previous CCW address)

The CSS then considers that newly fetched CCW for processing by
testing CCW flags as though in the following order:
* If the "suspend" (S) flag (`PCH_CCW_FLAG_SUSPEND`) is set, then
  the CSS "suspends" the channel program:
  - the application is notified (see below) just as it would be if
    the channel program ended but the CCW address and other fields
    in the schib are set so that they channel program can be resumed
    from its current position
  - with the channel program suspended, the application can inspect
    the schib and do whatever it likes, including updating CCWs in
    memory. It can then resume the channel program by calling
    `pch_sch_resume(sid)` and the CSS resumes the channel program
    from where it left off.
* If the "program controlled interruption" (PCI) flag
  (`PCH_CCW_FLAG_PROGRAM_CONTROLLED_INTERRUPTION`) is set, then the
  CSS triggers a notification to the application (see below) with
  the schib indicating the current channel program information
  (CCW address, status and so on) at this point but immediately
  continues executing the channel program (as described below) without
  stopping. The continuing channel program may (and probably will)
  cause the SCSW to be updated as it progresses so what the
  application sees, if it looks, will depend on when it looks.

Here, the CSS is going to "command-chain" and thus continue the
channel program. If the CCW command is a normal one (1-239) then
the CSS sends the command for execution just as at the start of
the channel program and the program continues in that fashion.

However, as well as those normal CCW commands that can be sent to
the device (as described at the beginning of the description of
CCWs and channel programs) there is an additional CCW command that
can be used when chaining: "Transfer In Channel" (TIC) which has
CCW command code decimal 240, hex 0xf0 (`PCH_CCW_CMD_TIC`).
(This specific command code value is different from that for
mainframe I/O.)

The CCW command TIC is the equivalent of a "goto" or "jump" for the
channel program and causes the CSS to get the memory address field
of the CCW (usually used as a data segment pointer) and treat it as
the memory address of the next CCW to be fetched. The CSS then fetches
that new CCW and continues the channel program from there, subject to
a few corner cases.

Note: It is valid (and common) to have a channel program with a loop
and it is valid to TIC to a TIC CCW but there are some corner case
conditions which are not yet checked and are probably not handled
correctly/sensibly by picochan at the moment.

### Channel program ending

When the channel program finishes, the CSS "notifies" the
application unless the application has chosen to avoid that
by setting various mask bits.

Similarly to how IRQs typically allow masking, enabling/disabling
and similarly in various ways, the CSS provides ways for the
application to choose how/when an event happening for a SCHIB
(such as a channel program ending) triggers notification.

## Notification to application

An event happens on a subchannel
* when a channel program ends
* when a device explicitly sends its device status to the CSS
and includes the "Alert" flag
* when the CSS fetches a CCW while progressing a channel program
and the CCW includes the "Suspend" (S) flag or the
"Program Controlled Interruption" (PCI) flag.

The application can either detect and manage these events using
API calls (see further down) or can arrange that the CSS notifies the
application when they happen by means of an asynchronous notification.

Such an asynchronous notification is via an "I/O Interuption" which,
on Pico, is implemented by the raising of the "CSS I/O IRQ". The
IRQ number for that is (must be) set at or after software CSS
initialisation time with the API call `pch_css_set_io_irq(io_irqnum)`.
The IRQ chosen should be one not used by any real peripheral - RP2040
and RP2350 have quite a few non-externally-connected IRQs that are
convenient for this purpose.

Each subchannel has an Interrupt Service Class ("ISC") which is
a 3-bit number (0-7) which defaults to 0. The ISC for a schib
is in the Path Management Control Word ("PMCW") field and can
be modified (at any time) with the general API call
`pch_sch_modify(sid,pmcw)` or its convenient more specific variant
`pch_sch_modify_isc(sid, iscnum)`.

A global 8-bit "I/O interruption mask" (one for each ISC)
determines whether a SCHIB with an I/O notification pending
actually raises the I/O IRQ. The mask can be set using API
call `pch_css_set_isc_enable_mask(mask)` and, as usual with
such an IRQ enablement mask, when a bit changes from 0 to 1
any pending SCHIBs with the ISC will cause the notification
to happen at that point.

Although an application could write and set an IRQ handler
itself to manage I/O interrupts, it may well instead want
to set the IRQ handler to the provided handler function
`pch_css_io_irq_handler` and set a callback function with
`pch_css_set_io_callback(io_callback_t io_callback)`
in which case schib notifications cause that provided handler
to retrieve the state information, clear down the notification
(see the "TEST SUBCHANNEL" function, `pch_sch_test()`) and call
the callback function with the interruption code (`pch_intcode_t`)
and SCSW (`pch_scsw_t`) as direct arguments.

Instead of getting an asynchronous notification by an I/O
interrupt or callback, an application can choose to retrieve and
reset the "notification pending" state of a subchannel itself -
this should be while the ISC for the subchannel is masked
(i.e. the bit in the ISC enablement mask for the subchannel's
ISC should be zero) or else there is a race condition when the
CSS handles the notification itself.

There are three main API calls related to inspecting and
resetting interruption conditions and related state for
subchannels. This state all resides in the SCSW part of the schib.
* `pch_sch_store(sid, schib)` fetches the current value of the
  schib and writes it to the `pch_schib_t *schib` pointer.
  The convenience function `pch_sch_store_scsw(sid, scsw)`
  just fetches the SCSW field of the schib and writes that to
  its pointer argument. Either way, this is a "look but not touch"
  API call which copies the SCSW (atomically) at the precise
  moment the function is called and no subchannel state is changed.
* `pch_sch_test(sid, scsw)` corresponds to the mainframe I/O
  instruction "TEST SUBCHANNEL" and this is the usual way to do deal
  with non-asynchronous notification of a subchannel but the naming
  is counter-intuitive. As well as fetching the current SCSW from
  the subchannel, it atomically tests to see whether the subchannel
  is in an "interruption condition" state and, if so, it _resets_
  that state
  - After calling `pch_sch_test` on a subchannel that is causing an
    interruption condition, the subchannel
    * is removed from the pending list and will no longer cause an
      I/O interruption, even if the ISC bit corresponding to the
      subchannel's ISC is re-enabled in the global ISC mask.
    * has the relevant parts of the SCSW cleared/reset so that it
      is no longer "status pending" - in particular, the
      `PCH_SC_STATUS_PENDING` flag is cleared from `ctrl_flags`
  - _Without_ calling `pch_sch_test` on a subchannel, a subchannel
    that is causing an interruption condition will remain in that
    state and, simply returning from the I/O interrupt handler will
    mean the handler is immediately re-entered.
  If the I/O interrupt handler is set to `pch_css_io_irq_handler`
  then that function calls `pch_sch_test` for you in order to
  retrieve the SCSW and call your callback function (set with
  `pch_css_set_io_callback`) with the retrieved SCSW. The other
  argument to your callback function is the `pch_intcode_t`
  that has the corresponding SID.
* `pch_test_pending_interruption()` (no arguments)
  tests whether any subchannel at all is currently causing an
  interrupt condition (whether masked or not). It should usually
  only be called when the ISC mask bits are disabled for the ISC
  of any subchannel that may possibly cause an interruption or
  else there is a race condition between the
  `pch_test_pending_interruption()` and the I/O interrupt handler
  being invoked. The type of the return value is `pch_intcode_t`
  which has two fields: a SID and a condition code (0-3).
  - If a subchannel is causing an interruption condition, the
    `pch_intcode_t` returned has its SID and cc=0. In this case,
    the interruption condition state is removed from the subchannel
    and it will no longer cause an interruption. However, the
    "status pending" and associated flags in the SCSW remain
    until inspected/cleared with `pch_sch_test`.
  - If no subchannel is causing an interruption condition, the
    `pch_intcode_t` returned has cc=1 (and the SID is zero but
    meaningless)
  - The order that subchannels are tested by
    `pch_test_pending_interruption` is in order of increasing ISC
    so subchannels with low ISC numbers have "higher priority" in
    terms of triggering interruption conditions than higher ISCs.

## Compiling using Picochan

Picochan is written in C using the Pico SDK. It uses CMake in the
way recommended by that SDK. Similarly to how the Pico SDK divides
into multiple modules with names of the form `pico_foo` and
`hardware_foo`, Picochan is divided into three CMake modules:
* `picochan_base`
* `picochan_css`
* `picochan_cu`

Application code need only compile with the `picochan_base` and
`picochan_css` modules.

Device driver code need only compile with the `picochan_base` and
`picochan_cu` modules.

This Doxygen-format Picochan documentation is in its early stages
and does not separate API information clearly enough from
internal implementation details. In an attempt to make some sort
of separation, much of the Doxygen documentation (generated from
code comments) has been marked as belonging to a "Doxygen topic"
with a name of either `picochan_base`, `picochan_css`, `picochan_cu`
or `internal_foo` (for various values of `foo`). The intent is that
any Doxygen topic with prefix `internal_` is not intended for API
use but there may be mis-classifications.

More documentation is needed on how to compile against Picochan
but in brief:
* Prepare your CMakeLists.txt in the usual way for Pico SDK
* Set environment variable `PICO_SDK_PATH` to the path of the
  Picochan library source - the `src/picochan` subtree of the
  Picochan repository. This path is the one which contains the
  top-level `CMakeLists.txt` file starting:

  ```
  if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/base/include/picochan/ccw.h)
  ```
  It is _not_ the root directory of the repository (which contains
  subdirectories "src" and "tools") nor is it the "base" subdirectory
  of the library source subtree which contains a `CMakeLists.txt`
  file starting:

  ```
  add_library(picochan_base INTERFACE)
  ```
* In your own `CMakeLists.txt` file for your software, add the
  following in appropriate places
  - Before any `target_compile_definitions` or similar, add
    ```
    include($ENV{PICOCHAN_PATH}/CMakeLists.txt)
    ```
  - In the `target_link_libraries` section for your target
    * add `picochan_css` if using application API (to CSS)
    * add `picochan_cu` if using device driver API (on CU)
    * add `hardware_dma` and `hardware_pio` if using any pio channels
    * add `hardware_uart` if using any uart channels
  - In the `target_compile_definitions` for your target, add any
    desired settings to any extra runtime sanity and argument checks
    for Debug builds or to enable tracing, such as
```
    PARAM_ASSERTIONS_ENABLED_PCH_CUS=1
    PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN=1
    PARAM_ASSERTIONS_ENABLED_PCH_TRC=1
    PARAM_ASSERTIONS_ENABLED_PCH_TXSM=1
    PARAM_ASSERTIONS_ENABLED_PCH_CSS=1
```
  - Add definitions to choose sizes for various global tables if the
    defaults are not suitable (and they may well not be).
    * Examples for using CSS:
```
    PCH_NUM_CSS_CUS=4
    PCH_NUM_SCHIBS=40
```
    * Examples for using CU:
```
    PCH_NUM_CUS=2
```

## Main types and API functions

Since there are not yet enough code comment Doxygen annotations to
divide the generated documentation into topics properly, here is a
brief list of the main definitions (e.g. types, macros and
API functions) for use by CSS-side and CU-side code.
They are described in the Doxygen-generated documentation but some
may be in a Topics sub-section, some may be in "Data Structures"
and some may be under "Files".

### Compile-time constants and definitions - examples:

```
#define PCH_NUM_CSS_CUS 4
#define PCH_NUM_SCHIBS 40
```

### Debugging assertions:

```
#define PARAM_ASSERTIONS_ENABLED_PCH_CSS 1
#define PARAM_ASSERTIONS_ENABLED_PCH_TXSM 1
```

### CSS-side usage

#### Types

```
typedef struct pch_schib pch_schib_t;

typedef struct pch_pmcw pch_pmcw_t;

typedef struct pch_intcode pch_intcode_t;

typedef void(*io_callback_t)(pch_intcode_t, pch_scsw_t);
```

#### Initialisation of whole CSS

```
void pch_css_init(void);

bool pch_css_set_trace(bool trace);

void pch_css_start(uint8_t dmairqix);

irq_set_exclusive_handler(schib_func_irqnum, pch_css_schib_func_irq_handler);
irq_set_enabled(schib_func_irqnum, true);
void pch_css_set_func_irq(irq_num_t irqnum);

void pch_css_set_io_irq(irq_num_t irqnum);

io_callback_t pch_css_set_io_callback(io_callback_t io_callback);

irq_set_exclusive_handler(io_irqnum, pch_css_io_irq_handler);
irq_set_enabled(io_irqnum, true);

void pch_css_set_isc_enable_mask(uint8_t mask);
```

#### Initialisation of each channel to a CU

```
void pch_css_cu_claim(pch_cunum_t cunum, uint16_t num_devices);
```

##### Initialise a channel to a UART CU

```
void pch_css_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl);
```

##### Initialise a channel to a memchan (cross-core) CU

```
void pch_memchan_init();

dmachan_tx_channel_t *pch_cus_cu_get_tx_channel(pch_cunum_t cunum);

void pch_css_memcu_configure(pch_cunum_t cunum, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer);
```

##### Initialise a channel to a pio CU

TBD

#### Start a channel to a CU

```
bool pch_css_set_trace_cu(pch_cunum_t cunum, bool trace);

void pch_css_cu_start(pch_cunum_t cunum);
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

### CU-side usage

#### Types

```
typedef struct pch_cu pch_cu_t;

typedef uint8_t pch_cbindex_t;

typedef struct pch_devib pch_devib_t;

typedef void (*pch_devib_callback_t)(pch_cu_t *cu, pch_devib_t *devib);

typedef struct pch_dev_sense pch_dev_sense_t;
```

#### Compile-time constants and definitions - examples:

```
#define NUM_CUS 2
```

#### Debugging assertions:

```
#define PARAM_ASSERTIONS_ENABLED_PCH_CUS 1
#define PARAM_ASSERTIONS_ENABLED_PCH_DMACHAN 1
#define PARAM_ASSERTIONS_ENABLED_PCH_TXSM 1
```

#### Initialisation of whole CU subsystem

```
void pch_cus_init(void);

bool pch_cus_set_trace(bool trace);

void pch_cus_init_dma_irq_handler(uint8_t dmairqix);

pch_cbindex_t pch_register_unused_devib_callback(pch_devib_callback_t cb);
```

#### Initialisation of each CU

```
void pch_cus_cu_init(pch_cu_t *cu, pch_cunum_t cunum, uint8_t dmairqix, uint16_t num_devibs);

bool pch_cus_trace_cu(pch_cunum_t cunum, bool trace);

void pch_cus_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl);

void pch_cus_cu_start(pch_cunum_t cunum);
```

#### Convenience API for device driver to its CU

##### Convenience API with fully general arguments

```
int pch_dev_set_callback(pch_cu_t *cu, pch_unit_addr_t ua, int cbindex_opt);
int pch_dev_send_then(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);
int pch_dev_send_zeroes_then(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);
int pch_dev_receive_then(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size, int cbindex_opt);
int pch_dev_update_status_advert_then(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, void *dstaddr, uint16_t size, int cbindex_opt);
```

##### Convenience API with some fixed arguments

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

#### Low-level API for device driver to its CU

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
