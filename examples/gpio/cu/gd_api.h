#ifndef _GD_API_H
#define _GD_API_H

#include <stdint.h>

#define GD_MAX_PIN 31

typedef struct gd_pins {
        uint8_t         base;   //!< GPIO number between 0 and 31
        uint8_t         count;  //!< value 0-7 representing count 1-8
} gd_pins_t;

typedef struct gd_filter {
        uint8_t         mask;
        uint8_t         target;
} gd_filter_t;

typedef struct gd_irq {
        uint8_t         pin;        //!< GPIO number between 0 and 31
        uint8_t         flags;
} gd_irq_t;

#define GD_IRQ_ENABLED  0x01
#define GD_IRQ_PENDING  0x02
#define GD_IRQ_FILTER   0x04

#define GD_IRQ_FLAGS_MASK       0x07

/*!
 * When the irq handler fires, it processes `flags` as follows:
 * - tests whether `GD_IRQ_FILTER` is set and, if so, reads the
 *   current values of the input pins, applies the filter
 *   condition and returns immediately if the match fails.
 * - if `GD_IRQ_FILTER` is not set or the condition succeeds, it
 *   sets the `GD_IRQ_PENDING` bit.
 * - if it has set `GD_IRQ_PENDING`, it checks to see if a channel
 *   program is running. If not, an unsolicited attention device status
 *   is generated.
 * 
 * When a channel program ends, if `GD_IRQ_PENDING` is set, the device
 * status includes the `PCH_DEVS_UNIT_EXCEPTION` flag.
 * The `GD_IRQ_PENDING` flag is not set back to 0 implicitly - the
 * application is responsible for updating the configuration register
 * to reset it when appropriate or else all subsequent channel programs
 * will end with UnitException status.
 */

//! CCW operation codes

//! PCH_CCW_CMD_WRITE (0x01) iterates through each written data
//! segment, processing one byte each `clock_period_us` microseconds,
//! setting the `out_pins` GPIOs to its value. GPIO pin `base` is
//! set to the low bit of the value and, when `count` is non-zero,
//! higher bits are set on pins `base+1` through `base+count`. Bits
//! of the byte higher than bit `count` are ignored.


//! PCH_CCW_CMD_READ (0x02) iterates through each offered data
//! segment, one byte each `clock_period_us` microseconds, reading
//! the `in_pins` GPIO`s and writing the result into the lower
//! bits of each byte.  Bits of the byte higher than bit `count`
//! are set to zero.


//! Read the `in_pins` GPIOs to produce an 8-bit value as in `READ`.
//! If a non-zero sized data segment is offered, write the value
//! to the first byte. Then, regardless of whether a data segment
//! was offered, test the `filter` condition against the value.
//! If there is a match, end the channel program with a device
//! status with the StatusModifier bit set so that the executing
//! CCW, if chaining, will skip the following CCW allowing for
//! conditional execution logic.
#define GD_CCW_CMD_TEST 0x04

/*! CCW operation codes to set configuration registers
 * 
 *  The following CCWs get and set the configuration registers.
 *  All values are little endian.  The SET_ CCWs read from the data
 *  segment the number of bytes corresponding to the size of the
 *  corresponding register and update it.  The GET_ CCWs read the
 *  value of the configuration register and write the value to the
 *  offered data segment.  All data must all be in the first data
 *  segment - data chaining is not supported. Any bytes beyond
 *  the size of the register are ignored by the device driver
 *  and thus will cause the CSS to cause a subchannel status with
 *  `PCH_SCHS_INCORRECT_LENGTH` for the channel program to deal
 *  with. If not enough bytes are provided, a device status including
 *  `PCH_DEVS_UNIT_CHECK` will be sent to end the channel program
 *  with an error and the available sense data will include the
 *  `PCH_DEV_SENSE_COMMAND_REJECT` flag with code `EINVALIDDATA`.
 * 
 *  Each configuration register has its own SET_ and GET_ CCWs with
 *  the the following command codes:
 */

#define GD_CCW_CMD_GET_CLOCK_PERIOD_US	 0xa0
#define GD_CCW_CMD_SET_CLOCK_PERIOD_US	 0xa1
#define GD_CCW_CMD_GET_OUT_PINS		 0xa2
#define GD_CCW_CMD_SET_OUT_PINS		 0xa3
#define GD_CCW_CMD_GET_IN_PINS		 0xa4
#define GD_CCW_CMD_SET_IN_PINS		 0xa5
#define GD_CCW_CMD_GET_FILTER		 0xa6
#define GD_CCW_CMD_SET_FILTER		 0xa7
#define GD_CCW_CMD_GET_IRQ_CONFIG	 0xa8
#define GD_CCW_CMD_SET_IRQ_CONFIG	 0xa9

#endif
