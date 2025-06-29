## Channel programs {#channel_program_page}

### Introduction

A _channel program_ is a series of (and often just one) 8-byte
_Channel Command Words_ (CCWs).

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
CSS/CU software. This is the the whole point of this software.

The data area for the device to read/write begins with the
`(address, count)` segment in that first CCW but can continue...
* if the "chain-data" flag (`PCH_CCW_FLAG_CD`) is set in the CCW flags
  field, then when the device exhausts that segment, the CSS fetches
  the next CCW in memory (next 8 bytes) and the device continues its
  reading/writing from/to the `(address,count)` in the new CCW.
  For that new ("data-chained") CCW, the command field is ignored.

When the data area is exhausted from that CCW command (the segment
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
  subchannel is notified. Some fields of the SCSW, including that for
  the device status, may not contain meaningful values at other times.

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
    in the schib are set so that the channel program can be resumed
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

It is valid (and common) to have a channel program with a loop
and it is valid to TIC to a TIC CCW but there are some corner case
conditions which are not yet checked and are probably not handled
correctly/sensibly by Picochan at the moment.

### Channel program ending

When the channel program finishes, the CSS "notifies" the
application unless the application has chosen to avoid that
by setting various mask bits.

Similarly to how IRQs typically allow masking and enabling/disabling
in various ways, the CSS provides ways for the application to choose
how/when an event happening for a SCHIB (such as a channel program
ending) triggers notification.

### Notification to application

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
`pch_sch_modify(sid, pmcw)` or its convenient more specific variant
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
`pch_css_set_io_callback(io_callback_t io_callback)`.
In this case, schib notifications cause that provided handler
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
  that state:
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

