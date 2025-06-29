## Design of Picochan {#design_page}

### Design

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

### CSS <-> CU protocol

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

