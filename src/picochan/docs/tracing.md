## Tracing {#tracing_page}

### Introduction

- Optional tracing to help debugging of CSS, CU and their users
- Code only present when compile-time `PCH_CONFIG_ENABLE_TRACE`
  `#define`d as non-zero
- Trace records are written for various events in CSS and CU
  when appropriate trace flags are set
- Trace records are small (~8-28 bytes) binary structs written
  to a small set of statically allocated buffers (a `bufferset`)
  that is treated as a ring buffer
- A bufferset is compile-time defined for one or both of CSS
  (if used) and CUs (if used) as, by default, 2 x 1KB buffers.
  Offloading traces for processing (see below) if the buffers
  are consecutive in memory (e.g. a single 2KB chunk).
- Trace records consist of a 48-bit timestamp (microseconds
  since boot), an 8-bit "trace record type" and an 8-bit count
  of associated data
- When each individual trace buffer becomes full, an IRQ can
  optionally be raised so that the application can fetch and
  offload that buffer's data before the other buffer(s) in the
  bufferset fill and the ring returns to restart writing to the
  just-filled buffer.
- The resulting data in the bufferset is expected to be
  offloaded and processed off-platform
- Offloading the necessary data can be done simply by using
  openocd or gdb (when SWD access is available) to fetch
    * the 32-byte metadata global variables `CSS.trace_bs` (CSS)
      or `pch_cus_trace_bs` (for CU)
    * the trace buffers themselves

### `pch_dump_trace` - parse and display traces (off-platform)

A `pch_dump_trace` program is provided - C source in the
`tools/pch_dump_trace.c` directory of the Picochan repository -
which is expected to be (compiled and) run off-platform
rather than on the Pico itself.

`pch_dump_trace` takes two filenames as input which are
expected to contain
  * the raw data from the 32-byte bufferset structure
  * the concetenated raw data from the trace buffers.
    This can simply be the contents of a single global
    `unsigned char pch_css_trace_buffer_space[]` array if
    there is room to have the buffers contigous (e.g. as 2KB
    instead of separate 1KB and 1KB)

`pch_dump_trace` parses the binary trace record data and, by default,
extracts the trace record fields and explains them in human-readable
output (although an option for raw "dump the timestamps, record type
name/numbers and data in hex format" is available).
For example, an extract from a basic test (with the UART channel
intentionally slowed to 1200 baud), reformatted slightly (to combine
the separate CSS and CU traces with indicators "+" for CSS-side and
"-" for CU-side):

```
01:02.707412 +CSS Function IRQ raised for CU=0 with pending UA=4 while tx_active=0
01:02.707441 +CSS CCW fetch for SID:0004 CCW address=20003000 provides CCW{cmd:03 flags:40 count=4 addr:20003300}
01:02.707474 +CSS-side SID:0004 sends packet{Start ua=4 CCWcmd:03 count=0(exact)}
01:02.707491 +CU tx channel DMAid=0 sets source to cmdbuf
01:02.707522 +IRQ for CSS-side CU=0 with DMA_IRQ_0 tx:irq_state=raised+complete,mem_src_state=idle rx:irq_state=none,mem_dst_state=idle
01:02.707537 +CSS-side CU=0 handling tx complete while txsm is idle
01:02.707565 +start subchannel SID:0004 CCW address=20003000 cc=0
01:02.707580 +test subchannel SID:0004 cc=1
01:02.707587 +test subchannel SID:0004 cc=1
01:02.743898 -IRQ for dev-side CU=0 with DMA_IRQ_1 tx:irq_state=none,mem_src_state=idle rx:irq_state=raised+complete,mem_dst_state=idle
01:02.743922 -dev-side CU=0 UA=4 received packet{Start ua=4 CCWcmd:03 count=0(exact)}
01:02.743953 -CU rx channel DMAid=3 sets destination to cmdbuf
01:02.743963 -dev-side CU=0 calls callback 1 for UA=4
01:02.744007 -dev-side CU=0 UA=4 sends packet{RequestRead ua=4 count=4}
01:02.744017 -CU tx channel DMAid=2 sets source to cmdbuf
01:02.744030 -IRQ for dev-side CU=0 with DMA_IRQ_1 tx:irq_state=raised+complete,mem_src_state=idle rx:irq_state=none,mem_dst_state=idle
01:02.744040 -dev-side CU=0 handling tx complete while txsm is idle
01:02.780445 +IRQ for CSS-side CU=0 with DMA_IRQ_0 tx:irq_state=none,mem_src_state=idle rx:irq_state=raised+complete,mem_dst_state=idle
01:02.780460 +CSS-side SID:0004 received packet{RequestRead ua=4 count=4}
01:02.780478 +CSS-side SID:0004 sends packet{Data|End ua=4 count=4}
```

### Interactive "offload trace buffers and parse/display" with gdb

For gdb, an example when the buffers are defined as a single
contiguous array:

```
unsigned char pch_css_trace_buffer_space[PCH_TRC_NUM_BUFFERS * PCH_TRC_BUFFER_SIZE] __aligned(4);

```
the following gdb definitions fetch and dump the current trace
buffers to host-local files and run the `pch_dump_trace` program
on the results (see below) to parse and display human-readable
explanations of the traced events:

```
define pch-show-css-trace
  dump binary value /tmp/gdb-css.bs CSS.trace_bs
  dump binary value /tmp/gdb-css.bufs pch_css_trace_buffer_space
  shell pch_dump_trace /tmp/gdb-css.bs /tmp/gdb-css.bufs
end
document pch-show-css-trace
  Dumps CSS trace buffers and uses pch_dump_trace to show them
end

define pch-show-cus-trace
  dump binary value /tmp/gdb-cus.bs pch_cus_trace_bs
  dump binary value /tmp/gdb-cus.bufs pch_cus_trace_buffer_space
  shell pch_dump_trace /tmp/gdb-cus.bs /tmp/gdb-cus.bufs
end
document pch-show-cus-trace
  Dumps CU trace buffers and uses pch_dump_trace to show them
end
```

### API to enable/disable trace at various levels

- The compile-time default is that no tracing code is present at all

- To include the ability to enable tracing,

```
#define PCH_CONFIG_ENABLE_TRACE 1
```

- To change the default of 2 x 1KB buffers in each bufferset
(where CSS, if present, uses one bufferset and CUs, if present,
use one bufferset between them), define `PCH_TRC_NUM_BUFFERS`
(default 2) and/or `PCH_TRC_BUFFER_SIZE` (default 1024) to
different compile-time constants, e.g.

```
#define PCH_TRC_NUM_BUFFERS 3
#define PCH_TRC_BUFFER_SIZE 2048
```

#### Tracing for CSS

- No trace records are written at all unless/until
`pch_css_set_trace(true)` is called, typically done
immediately after `pch_css_init()`. With this enabled, trace
records for CSS-global events are written.

- To enable trace records related to a given channel to be written,
call `pch_chp_set_trace(chpid, true)`. If the trace flag is not set
for a channel then no trace records for any subchannel on that
channel are written. With the trace flag for a channel enabled,
non-subchannel-specific trace records related to the channel are
written.

- To enable trace records related to a given subchannel, set the
`PCH_PMCW_TRACED` flag bit in the subchannel's `PMCW`, e.g. to set
the trace flag at the same time as subchannel `sid` is enabled:

```
uint16_t flags = PCH_PMCW_ENABLED | PCH_PMCW_TRACED;
pch_sch_modify_flags(sid, flags);
```

#### Tracing for CU

- No trace records are written at all unless/until
`pch_cus_set_trace(true)` is called, typically done
immediately after `pch_cus_init()`.

- To enable trace records related to a given CU and all its
devices to be written, call `pch_cus_trace_cu(cua, true)`.
Unlike for the CSS API, setting the trace flag at CU level
enables trace records for all its devices.

- To enable trace records related to a given device, set the
`PCH_DEVIB_FLAG_TRACED` flag bit in the `devib`, e.g. with
`pch_devib_set_traced(devib, true)`.
With the `PCH_DEVIB_FLAG_TRACED` bit present in the `flags`
field of a `devib` and the CU-global trace flag set
(with `pch_cus_set_trace()`), records will be written for events
related to the device regardless of whether the trace flag for
its CU has been set with `pch_cus_trace_cu(cua, val)`.
