## Example CU device driver for GPIOs

### Introduction

This example is a CU device driver which allows channel programs
running in a remote CSS to:
- write clocked runs of bits to a set of output GPIOs
- read clocked runs from a set of input GPIOs
- generate an unsolicited alert for a GPIO treated as an interrupt,
  optionally 
- do basic a filtering test of a set of input GPIOs,
  `(value & mask == target)` which can be used in two ways:
    * for conditional logic in channel programs by using the device
      status StatusModifier bit (set when the filter matches)
    * to restrict generation of an unsolicited alert to when the
      set of input GPIOs read at interrupt time satisfy the filter

To keep this example simple
- the sets of input and output GPIOs (which can be distinct) are
  given by a base GPIO number between 0 and 31 (so only the lower
  set of GPIOs on RP2350 is accessible)
- the count of each set is between 1 and 8 so that each read/write
  is sent/returned as a single byte
- clocked runs of GPIO data are only read/written while a channel
  program is running (rather than allowing caching or overlap of
  reading/writing GPIO values from the CU with application processing
  in between channel programs)

### Programmer's Model

Each gpio_dev device has its own gpio_dev_config struct whose member
fields behave like "configuration registers" that can be written
and read via CCWs:

```

typedef struct pins_range {
        uint8_t         base;   //!< GPIO number between 0 and 31
        uint8_t         count;  //!< value 0-7 representing count 1-8
} pins_range_t;

typedef struct filter {
        uint8_t         mask;
        uint8_t         target;
} filter_t;

typedef struct irq_config {
        uint8_t         pin;        //!< GPIO number between 0 and 31
        uint8_t         flags;
} irq_config_t;

#define GD_IRQ_ENABLED  0x01
#define GD_IRQ_PENDING  0x02
#define GD_IRQ_FILTER   0x04

typedef struct gpio_dev_config {
        uint32_t        clock_period_us;
        pins_range_t    out_pins;
        pins_range_t    in_pins;
        filter_t        filter;
        irq_config_t    irq;
} gpio_dev_config_t;

- `clock_period_us`: a 32-bit count of microseconds for the delay
  between each successive 8-bit value read from the set of input pins
  or written to the set of output pins
- `out_pins`: a consecutive range of 1-8 GPIO pins affected by
  WRITE and similar CCWs
- `in_pins`: a consecutive range of 1-8 GPIO pins read by
  READ and similar CCWs
- `filter`: a boolean condition that can be tested against an 8-bit
  value, typically the value read from the set of input pins. The
  condition is true when `(value & mask) == target`.
- `irq`: when the `GD_IRQ_ENABLED` of `flags` is changed from
  0 to 1, an irq handler is set for `pin` - see below; when it is
  changed from 1 to 0, the handler is removed.
  When the `GD_IRQ_FILTER` bit is set in `flags`, the `filter`
  condition is tested during the irq handler - see below.

When the irq handler fires, it processes `flags` as follows:
- tests whether `GD_IRQ_FILTER` is set and, if so, reads the
  current values of the input pins, applies the filter
  condition and returns immediately if the match fails.
- if `GD_IRQ_FILTER` is not set or the condition succeeds, it
  sets the `GD_IRQ_PENDING` bit.
- if it has set `GD_IRQ_PENDING`, it checks to see if a channel
  program is running. If not, an unsolicited attention device status
  is generated.

When a channel program ends, if `GD_IRQ_PENDING` is set, the device
status includes the `PCH_DEVS_UNIT_EXCEPTION` flag.
The `GD_IRQ_PENDING` flag is not set back to 0 implicitly - the
application is responsible for updating the configuration register
to reset it when appropriate or else all subsequent channel programs
will end with UnitException status.

### CCW operation codes:

#### WRITE (cmd 0x01)

Iterates through each written data segment, processing one byte each
`clock_period_us` microseconds, setting the `out_pins` GPIOs
to its value. GPIO pin `base` is set to the low bit of the value and,
when `count` is non-zero, higher bits are set on pins `base+1` through
`base+count`. Bits of the byte higher than bit `count` are ignored.

#### READ (cmd 0x02)

Iterates through each offered data segment, one byte each
`clock_period_us` microseconds, reading the `in_pins` GPIO`s
and writing the result into the lower bits of each byte.
Bits of the byte higher than bit `count` are set to zero.

#### TEST (cmd 0x04)

Read the `in_pins` GPIOs to produce an 8-bit value as in `READ`.
If a non-zero sized data segment is offered, write the value to
the first byte. Then, regardless of whether a data segment was
offered, test the `filter` condition against the value.
If there is a match, end the channel program with a device status
with the StatusModifier bit set so that the executing CCW,
if chaining, will skip the following CCW allowing for conditional
execution logic.

#### Setting configuration registers

The following CCWs get and set the configuration registers.
All values are little endian.  The SET_ CCWs read from the data
segment the number of bytes corresponding to the size of the
corresponding register and update it.  The GET_ CCWs read the value
of the configuration register and write the value to the offered
data segment.  All data must all be in the first data segment -
data chaining is not supported. Any bytes beyond the size of the
register are ignored by the device driver and thus will cause the
CSS to cause a subchannel status with `PCH_SCHS_INCORRECT_LENGTH` for
the channel program to deal with. If not enough bytes are provided,
a device status including `PCH_DEVS_UNIT_CHECK` will be sent to
end the channel program with an error and the available sense data
will include the `PCH_DEV_SENSE_COMMAND_REJECT` flag with code
`EINVALIDDATA`.

Each configuration register has its own SET_ and GET_ CCWs
with the the following command codes:

- `GET_CLOCK_PERIOD_US` (cmd 0xa0)
- `SET_CLOCK_PERIOD_US` (cmd 0xa1)
- `GET_OUT_PINS` (cmd 0xa2)
- `SET_OUT_PINS` (cmd 0xa3)
- `GET_IN_PINS` (cmd 0xa4)
- `SET_IN_PINS` (cmd 0xa5)
- `GET_FILTER` (cmd 0xa6)
- `SET_FILTER` (cmd 0xa7)
- `GET_IRQ_CONFIG` (cmd 0xa8)
- `SET_IRQ_CONFIG` (cmd 0xa9)
