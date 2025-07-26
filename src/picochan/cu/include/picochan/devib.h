/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CU_DEVIB_H
#define _PCH_CU_DEVIB_H

#include "pico/platform/compiler.h"
#include "picochan/ids.h"
#include "picochan/dev_status.h"
#include "picochan/dev_sense.h"
#include "proto/chop.h"
#include "proto/payload.h"

/*! \file picochan/devib.h
 *  \ingroup picochan_cu
 *
 * \brief The structures and API for a device on a CU
 */

/*! \brief An 8-bit index into an array of callbacks that the CU can make to a device
 *  \ingroup picochan_cu
 * pch_cbindex_t is an 8-bit index into pch_devib_callbacks, an array
 * of up to NUM_DEVIB_CALLBACKS registered callbacks on devibs.
 */
typedef uint8_t pch_cbindex_t;

#define PCH_DEVIB_CALLBACK_DEFAULT      0
#define PCH_DEVIB_CALLBACK_NOOP         255

/*! \brief The maximum number of registered callbacks
 *  \ingroup picochan_cu
 *
 * A callback index greater than this is handled internally.
 */
#define MAX_DEVIB_CALLBACKS 254

/*! \brief The size of the global callbacks array
 *  \ingroup picochan_cu
 *
 * Must be a compile-time definition, must not exceed
 * MAX_DEVIB_CALLBACKS (254) and must provide room for any internal
 * specially-defined callbacks. Default 16.
 */
#define NUM_DEVIB_CALLBACKS 16
static_assert(NUM_DEVIB_CALLBACKS <= MAX_DEVIB_CALLBACKS,
        "NUM_DEVIB_CALLBACKS must not exceed MAX_DEVIB_CALLBACKS");

static_assert(sizeof(pch_dev_sense_t) == 4,
        "pch_dev_sense_t must be 4 bytes");

/*! \brief pch_devib_t represents a device on a CU
 *  \ingroup picochan_cu
 *
\verbatim
DEVIB  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |     next      |    cbindex    |          size                 |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |       op      |     flags     |         payload               |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                          bufaddr                              |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                            sense                              |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
\endverbatim
 */
typedef struct __aligned(4) pch_devib {
        pch_unit_addr_t next;
        pch_cbindex_t   cbindex;
        uint16_t        size;
        proto_chop_t    op;
        uint8_t         flags;
        proto_payload_t payload;
        uint32_t        addr;
        pch_dev_sense_t sense;
} pch_devib_t;

#define PCH_DEVIB_SPACE_SHIFT (31U - __builtin_clz(2 * sizeof(pch_devib_t) - 1))

static_assert(__builtin_constant_p(PCH_DEVIB_SPACE_SHIFT),
        "__builtin_clz() did not produce compile-time constant for PCH_DEVIB_SPACE_SHIFT");

#define PCH_DEVIB_FLAG_STARTED          0x80
#define PCH_DEVIB_FLAG_CMD_WRITE        0x40
#define PCH_DEVIB_FLAG_RX_DATA_REQUIRED 0x20
#define PCH_DEVIB_FLAG_TX_CALLBACK      0x10
#define PCH_DEVIB_FLAG_TRACED           0x08

static inline bool pch_devib_is_started(pch_devib_t *devib) {
        return devib->flags & PCH_DEVIB_FLAG_STARTED;
}

static inline bool pch_devib_is_cmd_write(pch_devib_t *devib) {
        return devib->flags & PCH_DEVIB_FLAG_CMD_WRITE;
}

static inline bool pch_devib_is_traced(pch_devib_t *devib) {
        return devib->flags & PCH_DEVIB_FLAG_TRACED;
}

static inline bool pch_devib_set_traced(pch_devib_t *devib, bool trace) {
        bool old_trace = pch_devib_is_traced(devib);
        if (trace)
                devib->flags |= PCH_DEVIB_FLAG_TRACED;
        else
                devib->flags &= ~PCH_DEVIB_FLAG_TRACED;

        return old_trace;
}

// Forward declaration of pch_cu_t for identifying devib by
// (pch_cu_t, pch_unit_addr_t) for callbacks and dev implementations.
typedef struct pch_cu pch_cu_t;

// Callbacks

/*! \brief pch_devib_callback_t is a function for the CU to callback a device
 *  \ingroup picochan_cu
 */
typedef void (*pch_devib_callback_t)(pch_cu_t *cu, pch_devib_t *devib);

extern pch_devib_callback_t pch_devib_callbacks[];

static inline bool pch_cbindex_is_callable(uint cbindex) {
        if (cbindex == PCH_DEVIB_CALLBACK_NOOP)
                return true;

        if (cbindex >= NUM_DEVIB_CALLBACKS)
                return false;

        return pch_devib_callbacks[cbindex] != NULL;
}

// Callback registration API

/*! \brief Registers a device callback function at a specific index
 *  \ingroup picochan_cu
 *
 * For a Debug build, asserts if n is out of range in the global
 * array of callbacks or if the callback index is already registered.
 */
void pch_register_devib_callback(pch_cbindex_t n, pch_devib_callback_t cb);

/*! \brief Registers a device callback function at an unused index
 *  \ingroup picochan_cu
 *
 * Panics if no more unused indices are available in the global
 * array of callbacks. This performs a simple linear iteration of
 * the array to find the first unused slot so is not intended to be
 * used at performance sensitive times.
 *
 * \return The allocated callback index number
 */
pch_cbindex_t pch_register_unused_devib_callback(pch_devib_callback_t cb);

void pch_default_devib_callback(pch_cu_t *cu, pch_devib_t *devib);

// Low-level API for dev implementation updating devib

/*! \brief Low-level API to update devib->cbindex
 *  \ingroup picochan_cu
 *
 * The cbindex field determines the callback that the CU will invoke
 * the next time an event happens that needs handling by the device.
 * For a Debug build, asserts if cbindex is invalid (out of range
 * or unregistered).
 * 
 * Typically, device driver authors should use the higher-level
 * pch_dev_ API rather than this low-level API.
 */
static inline void pch_devib_prepare_callback(pch_devib_t *devib, pch_cbindex_t cbindex) {
        assert(pch_cbindex_is_callable(cbindex));
        devib->cbindex = cbindex;
}

/*! \brief Low-level API to update devib->payload with a count field
 *  \ingroup picochan_cu
 *
 * The payload of a RequestRead or Data channel operation command
 * provides the count of data bytes that are requested from the
 * channel or are to be sent to the channel.
 * 
 * Typically, device driver authors should use the higher-level
 * pch_dev_ API rather than this low-level API.
 */
static inline void pch_devib_prepare_count(pch_devib_t *devib, uint16_t count) {
        devib->payload = proto_make_count_payload(count);
}

/*! \brief Low-level API to prepare a Data channel operation command for a device
 *  \ingroup picochan_cu
 *
 * Uses pch_devib_prepare_count to set the count of bytes to be
 * written, sets the source address for the bytes and sets the
 * channel operation command to be PROTO_CHOP_DATA along with any
 * provided flags.
 *
 * For a Debug build, asserts if the device has not received a
 * Start operation.
 * 
 * Typically, device driver authors should use the higher-level
 * pch_dev_ API rather than this low-level API.
 */
static inline void pch_devib_prepare_write_data(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags) {
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        pch_devib_prepare_count(devib, n);
        devib->op = PROTO_CHOP_DATA | flags;
        devib->addr = (uint32_t)srcaddr;
}

/*! \brief Low-level API to prepare a Data channel operation command for a device that will implicitly send zeroes
 *  \ingroup picochan_cu
 *
 * Uses pch_devib_prepare_count to set the count of zero bytes to be
 * written and sets the channel operation command to be
 * PROTO_CHOP_DATA together with the PROTO_CHOP_FLAG_SKIP flag that
 * means that the CU does not have to send any actual data bytes
 * down the channel and causes the CSS to write zero bytes itself
 * directly to the CCW's destination address.
 *
 * For a Debug build, asserts if the device has not received a
 * Start operation.
 * 
 * Typically, device driver authors should use the higher-level
 * pch_dev_ API rather than this low-level API.
 */
static inline void pch_devib_prepare_write_zeroes(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags) {
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        pch_devib_prepare_count(devib, n);
        // hard-code the ResponseRequired flag for now
        devib->op = PROTO_CHOP_DATA | PROTO_CHOP_FLAG_SKIP | flags;
}

/*! \brief Low-level API to prepare a RequestRead channel operation command for a device
 *  \ingroup picochan_cu
 *
 * Uses pch_devib_prepare_count to set the count of bytes that are
 * to be requested, sets the destination address for the bytes and
 * sets the channel operation command to be PROTO_CHOP_REQUEST_READ.
 *
 * For a Debug build, asserts if the device has not received a
 * Start operation.
 * 
 * Typically, device driver authors should use the higher-level
 * pch_dev_ API rather than this low-level API.
 */
static inline void pch_devib_prepare_read_data(pch_devib_t *devib, void *dstaddr, uint16_t size) {
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        pch_devib_prepare_count(devib, size);
        devib->op = PROTO_CHOP_REQUEST_READ;
        devib->flags |= PCH_DEVIB_FLAG_RX_DATA_REQUIRED;
        devib->addr = (uint32_t)dstaddr;
}

/*! \brief Low-level API to prepare an UpdateStatus channel operation command for a device
 *  \ingroup picochan_cu
 *
 * Sets the channel operation command to be PROTO_CHOP_UDPATE_STATUS.
 * Sets the device status (devs) in the payload.
 * If it's either an unsolicited status (neither ChannelEnd
// nor DeviceEnd set) or it's end-of-channel-program (both ChannelEnd
// and DeviceEnd set) then it also sets the devib addr field to
// dstaddr, the size field to field and encodes the (16-bit) size as
// an 8-bit "bsize" value within the payload. A non-zero value of
// the size advertises to the CSS the buffer and length to which the
// next CCW Write-type command can immediately send data during Start.
 *
 * For a Debug build, asserts if the device has not received a
 * Start operation.
 * 
 * Typically, device driver authors should use the higher-level
 * pch_dev_ API rather than this low-level API.
 */
void pch_devib_prepare_update_status(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size);

void pch_devib_send_or_queue_command(pch_cu_t *cu, pch_unit_addr_t ua);
#endif
