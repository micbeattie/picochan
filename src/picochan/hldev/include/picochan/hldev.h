/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_HLDEV_HLDEV_H
#define _PCH_HLDEV_HLDEV_H

#include "picochan/cu.h"

/*! \file picochan/hldev.h
 *  \defgroup picochan_hldev picochan_hldev
 *
 * \brief A higher level API for implementing devices on a CU
 *
 * For example,
 *
 * ```
 * typedef struct my_dev {
 *      pch_hldev_t             hldev; // must be first
 *      foo_t                   foo; // my device-specific fields
 * } my_dev_t;
 *
 * typedef struct my_cu_config {
 *      pch_hldev_config_t      hldev_config; // must be first
 *      bar_t                   bar; // my_cu-specific fields
 *      my_dev_t                mydevs[NUM_MYDEVS];
 * } my_cu_config_t;
 *
 * static pch_hldev_t *my_get_hldev(pch_hldev_config_t *hdcfg, int i) {
 *      my_cu_config_t *cfg = (my_cu_config_t *)hdcfg;
 *      return &cfg->mydevs[i].hldev;
 * };
 *
 * void my_start(pch_devib_t *devib) {
 *      // if you only need the pch_hldev_t...
 *      pch_hldev_t hd = pch_hldev_get(devib);
 *      // ...or if you need your extra device fields...
 *      my_dev_t md = (my_dev_t *)pch_hldev_get(devib);
 *      // do something to process CCW command hd->ccwcmd
 *      // using pch_hldev_send...(devib, ...) to send data to
 *      // a Read-type CCW or pch_hldev_receive...(devib, ...) to
 *      // to receive data from a Write-type CCW. End the channel
 *      // program with pch_hldev_end_...(devib, ...).
 * }
 *
 * my_cu_config_t the_my_cu_config = {
 *      .hldev_config = {
 *              .get_hldev = my_get_hldev,
 *              .start = my_start_callback
 *      }
 * };
 *
 * pch_unit_addr_t my_cu_init(pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices) {
 *      pch_hldev_config_init(&the_my_cu_config.hldev_config, cu, first_ua, num_devices);
 *      return first_ua + num_devices;
 *  }
 *  ```
 */

// values for pch_hldev_t state field
#define PCH_HLDEV_IDLE          0
#define PCH_HLDEV_STARTED       1
#define PCH_HLDEV_RECEIVING     2
#define PCH_HLDEV_SENDING       3
#define PCH_HLDEV_SENDING_FINAL 4
#define PCH_HLDEV_ENDING        5

// values for code fields of dev_sense_t for PCH_DEV_SENSE_PROTO_ERROR
#define PCH_HLDEV_ERR_NO_START_CALLBACK         1
#define PCH_HLDEV_ERR_RECEIVE_FROM_READ_CCW     2
#define PCH_HLDEV_ERR_SEND_TO_WRITE_CCW         3
#define PCH_HLDEV_ERR_IDLE_OP_NOT_START         4

typedef struct pch_hldev_config pch_hldev_config_t;
typedef struct pch_hldev pch_hldev_t;

/*! \brief Driver-provided pch_hldev_t lookup callback
 *  \ingroup picochan_hldev
 *
 * This is the type used by the get_hldev field of
 * pch_hldev_config_t. It is a driver-provided function called by
 * the hldev subsystem which must return a pointer to the
 * pch_hldev_t corresponding to the device with index i (not the
 * devib with unit address i) within the hdcfg device range.
 */
typedef pch_hldev_t *(*pch_hldev_getter_t)(pch_hldev_config_t *hdcfg, int i);

/*! \brief pch_hldev_config_t represents a range of devices on a CU
 *  that is to be used with the hldev API.
 *  \ingroup picochan_hldev
 *
 * Fill in get_hldev and start (and, optionally, signal) and call
 * pch_hldev_config_init() to register a range of devices for a CU.
 */
typedef struct pch_hldev_config {
        pch_dev_range_t         dev_range;
        pch_hldev_getter_t      get_hldev;
        pch_devib_callback_t    start;
        pch_devib_callback_t    signal;
} pch_hldev_config_t;

/*! \brief Convenience inline function to return the CU of the hdcfg.
 *  \ingroup picochan_hldev
 */
static inline pch_cu_t *pch_hldev_config_get_cu(pch_hldev_config_t *hdcfg) {
        return hdcfg->dev_range.cu;
}

/*! \brief pch_hldev_t represents a device controlled by the hldev API.
 *  \ingroup picochan_hldev
 *
 * The get_hldev callback function in the pch_hldev_config_t, hdcfg,
 * must locate the appropriate pch_hldev_t given its index number
 * within the dev_range of hdcfg. Typically, this is simply by
 * indexing into a pre-defined array of structs, each of which starts
 * with (or, in the most simple case, is) a pch_hldev_t.
 */
typedef struct pch_hldev {
        pch_devib_callback_t    callback;
        void                    *addr;  // dest/source address for receive/send
        uint16_t                size;   // total bytes to receive/send
        uint16_t                count;  // bytes received/sent so far
        uint8_t                 state;
        uint8_t                 flags;
        uint8_t                 ccwcmd;
} pch_hldev_t;

// values for pch_hldev_t flags
// PCH_HLDEV_FLAG_EOF indicates that no more data is available to be
// received from a Write-type CCW
#define PCH_HLDEV_FLAG_EOF      0x01
// PCH_HLDEV_FLAG_TRACED indicates that trace records will be written
// for events for this hldev
#define PCH_HLDEV_FLAG_TRACED   0x02

static inline bool pch_hldev_is_idle(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_IDLE;
}

static inline bool pch_hldev_is_started(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_STARTED;
}

static inline bool pch_hldev_is_receiving(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_RECEIVING;
}

static inline bool pch_hldev_is_sending(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_SENDING;
}

static inline bool pch_hldev_is_sending_final(pch_hldev_t *hd) {
        return hd->state == PCH_HLDEV_SENDING_FINAL;
}

static inline bool pch_hldev_is_traced(pch_hldev_t *hd) {
        return hd->flags & PCH_HLDEV_FLAG_TRACED;
}

static inline void pch_hldev_set_traced(pch_hldev_t *hd, bool b) {
        if (b)
                hd->flags |= PCH_HLDEV_FLAG_TRACED;
        else
                hd->flags &= ~PCH_HLDEV_FLAG_TRACED;
}

static inline pch_hldev_config_t *pch_hldev_get_config(pch_devib_t *devib) {
        return (pch_hldev_config_t *)pch_devib_callback_context(devib);
}

/*! \brief Look up the index number of this device within the
 *  dev_range of its owning pch_hldev_config_t.
 *  \ingroup picochan_hldev
 *
 * devib must be owned by a pch_hldev_config_t. Returns a -1 if
 * the devib is not in the range (shouldn't happen).
 */
static inline int pch_hldev_get_index(pch_devib_t *devib) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        return pch_dev_range_get_index(&hdcfg->dev_range, devib);
}

/*! \brief Look up the index number of this device within the
 *  dev_range of its owning pch_hldev_config_t.
 *  \ingroup picochan_hldev
 *
 * devib must be owned by a pch_hldev_config_t. panics if the devib
 * is not in the range (shouldn't happen).
 */
static inline int pch_hldev_get_index_required(pch_devib_t *devib) {
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        return pch_dev_range_get_index_required(&hdcfg->dev_range, devib);
}

/*! \brief Look up the pch_hldev_t corresponding to device devib.
 *  \ingroup picochan_hldev
 *
 * devib must be owned by a pch_hldev_config_t. Returns NULL if
 * the devib is not in the range (shouldn't happen).
 */
static inline pch_hldev_t *pch_hldev_get(pch_devib_t *devib) {
        int i = pch_hldev_get_index(devib);
        if (i == -1)
                return NULL;

        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        return hdcfg->get_hldev(hdcfg, i);
}

/*! \brief Look up the pch_hldev_t corresponding to device devib.
 *  \ingroup picochan_hldev
 *
 * devib must be owned by a pch_hldev_config_t. Panics if the devib
 * is not in the range (shouldn't happen).
 */
static inline pch_hldev_t *pch_hldev_get_required(pch_devib_t *devib) {
        int i = pch_hldev_get_index_required(devib);
        pch_hldev_config_t *hdcfg = pch_hldev_get_config(devib);
        return hdcfg->get_hldev(hdcfg, i);
}

static inline pch_devib_t *pch_hldev_get_devib(pch_hldev_config_t *hdcfg, int i) {
        return pch_dev_range_get_devib_by_index_required(&hdcfg->dev_range, i);
}

/*! \brief Receive data offered by the current (Write-type) CCW and
 *  write it to dstaddr.
 *  \ingroup picochan_hldev
 *
 * hldev requests as much data as possible up to size bytes, issuing
 * multiple ReadRequest channel operations if needed as the CSS
 * chains through any additional data-chained buffer segments.
 * The receive stops when either size bytes are received or the CSS
 * has no more bytes to provide, either because all chained segments
 * offered are exhausted or because a Halt Subchannel has stopped the
 * channel program. Afterwards, the hldev's current callback is
 * replaced with callback (if non-NULL) and the (potentially updated)
 * callback is called. The actual number of bytes received and
 * written to dstaddr is available in the count field of the
 * pch_hldev_t. If no more data is available to be received, with
 * count either less than or equal to count, then the pch_hldev_t
 * flags field has PCH_HLDEV_FLAG_EOF set.
 */
void pch_hldev_receive_then(pch_devib_t *devib, void *dstaddr, uint16_t size, pch_devib_callback_t callback);

/*! \brief Receive data offered by the current (Write-type) CCW and
 *  write it to dstaddr.
 *  \ingroup picochan_hldev
 *
 *  Does the same as pch_hldev_receive_then(), passing NULL as the
 *  callback argument so that the current callback is not changed.
 */
void pch_hldev_receive(pch_devib_t *devib, void *dstaddr, uint16_t size);

void pch_hldev_call_callback(pch_devib_t *devib);

/*! \brief Reads data from srcaddr and sends it the current
 * (Read-type) CCW.
 *  \ingroup picochan_hldev
 *
 *  hldev sends as much data as possible up to size bytes, issuing
 *  multiple Data channel operations if needed as the CSS chains
 *  through any additional data-chained buffer segments.
 *  The send stops when either size bytes have been sent or the CSS
 *  has no more space to offer because all chained segments have been
 *  exhausted. Afterwards, the hldev's current callback is replaced
 *  with callback (if non-NULL) and the (potentially updated) callback
 *  is called. The actual number of bytes sent from srcaddr is
 *  available in the count field of the pch_hldev_t.
 */
void pch_hldev_send_then(pch_devib_t *devib, void *srcaddr, uint16_t size, pch_devib_callback_t callback);

/*! \brief Calls pch_hldev_send() then pch_hldev_end_ok().
 *  \ingroup picochan_hldev
 */
void pch_hldev_send_final(pch_devib_t *devib, void *srcaddr, uint16_t size);

/*! \brief Reads data from srcaddr and sends it the current
 *  (Read-type) CCW.
 *  \ingroup picochan_hldev
 *
 *  Does the same as pch_hldev_send_then(), passing NULL as the
 *  callback argument so that the current callback is not changed.
 */
void pch_hldev_send(pch_devib_t *devib, void *srcaddr, uint16_t size);

/*! \brief Ends the current channel program
 *  \ingroup picochan_hldev
 *
 *  Sends an UpdateStatus channel operation to the CSS to end the
 *  current channel program. The device status sent always includes
 *  ChannelEnd|DeviceEnd (which is what ends the channel program)
 *  and will also set any additional flags given in extra_devs.
 *  sense is written to the sense field of the devib so that is
 *  available to satisfy a PCH_CCW_CMD_SENSE CCW with no neeed to
 *  bother the device driver.
 */
void pch_hldev_end(pch_devib_t *devib, uint8_t extra_devs, pch_dev_sense_t sense);

/*! \brief Ends the current channel program with normal status
 *  \ingroup picochan_hldev
 *
 *  Does the same as pch_hldev_end(), passing 0 as extra_devs and
 *  PCH_DEV_SENSE_NONE (zeroes) as the sense.
 */
void pch_hldev_end_ok(pch_devib_t *devib);

/*! \brief Appends a \0 to the buffer of the hldev of devib.
 *  \ingroup picochan_hldev
 *
 *  Looks up the pch_hldev_t of devib, writes a \0 to its addr
 *  pointer field and increments its count field. Intended to be
 *  used as a convenience function during a callback in a
 *  Read-Type channel program where pch_hldev_receive_then() has
 *  been  called to receive counted data bytes but NUL-termination
 *  is wanted.
 */
void pch_hldev_terminate_string(pch_devib_t *devib);

/*! \brief Does pch_hldev_terminate_string() then pch_hldev_end_ok().
 *  \ingroup picochan_hldev
 *
 * Intended to be used as the callback argument of a
 * pch_hldev_receive_then() so that, after receiving as many bytes
 * as possible, hldev terminates the resulting buffer with a \0
 * (for which the caller is responsible for ensuring room is
 * available) and then ending the channel program with no further
 * callbacks needed.
 */
void pch_hldev_terminate_string_end_ok(pch_devib_t *devib);

/*! \brief Does pch_hldev_receive() then pch_hldev_end_ok().
 *  \ingroup picochan_hldev
 *
 *  Receives data into the hldev's buffer then ends the channel
 *  program with normal status with no further callbacks needed.
 */
void pch_hldev_receive_buffer_final(pch_devib_t *devib, void *dstaddr, uint16_t size);

/*! \brief Does pch_hldev_receive() then
 *  pch_hldev_terminate_string_end_ok().
 *  \ingroup picochan_hldev
 *
 *  Receives data into the hldev's buffer, appends a trailing \0
 *  then ends the channel program with normal status with no
 *  further callbacks needed.
 */
void pch_hldev_receive_string_final(pch_devib_t *devib, void *dstaddr, uint16_t len);

/*! \brief Ends the current channel program with normal status
 *  and sets the sense code
 *  \ingroup picochan_hldev
 *
 * Does pch_hldev_end(), passing 0 as the extra_devs.
 */
static inline void pch_hldev_end_ok_sense(pch_devib_t *devib, pch_dev_sense_t sense) {
        pch_hldev_end(devib, 0, sense);
}

/*! \brief Ends the current channel program with a Command Reject
 *  error.
 *  \ingroup picochan_hldev
 *
 *  Does pch_hldev_end(), passing device status as an error where
 *  UnitCheck set and an associated sense of CommandReject with sense
 *  code code. This error signifies that the CCW command was invalid
 *  or that, for a Write-type CCW, data that it sent was invalid.
 */
static inline void pch_hldev_end_reject(pch_devib_t *devib, uint8_t code) {
        pch_hldev_end(devib, 0, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_COMMAND_REJECT,
                .code = code
        }));
}

/*! \brief Ends the current channel program with UnitException
 *  and sets an explicit sense.
 *  \ingroup picochan_hldev
 *
 *  Does pch_hldev_end(), passing device status with the
 *  UnitException flag sent and setting the given sense.
 *  A UnitException is not an error but causes the channel program to
 *  end without command chaining. The intent for UnitException for
 *  mainframe channel programs is that a given device only has a
 *  single meaning for UnitException.
 */
static inline void pch_hldev_end_exception_sense(pch_devib_t *devib, pch_dev_sense_t sense) {
        pch_hldev_end(devib, PCH_DEVS_UNIT_EXCEPTION, sense);
}

/*! \brief Ends the current channel program with UnitException
 *  and no sense information.
 *  \ingroup picochan_hldev
 *
 *  Does pch_hldev_end_exception_sense(), passing PCH_DEV_SENSE_NONE
 *  as the sense information.
 */
static inline void pch_hldev_end_exception(pch_devib_t *devib) {
        pch_hldev_end_exception_sense(devib, PCH_DEV_SENSE_NONE);
}

/*! \brief Ends the current channel program with an
 *  InterventionRequired error.
 *  \ingroup picochan_hldev
 *
 *  Does pch_hldev_end(), setting UnitCheck in the device status
 *  and InterventionRequired in the sense.
 */
static inline void pch_hldev_end_intervention(pch_devib_t *devib, uint8_t code) {
        pch_hldev_end(devib, 0, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_INTERVENTION_REQUIRED,
                .code = code
        }));
}

/*! \brief Ends the current channel program with an
 *  EquipmentCheck error.
 *  \ingroup picochan_hldev
 *
 *  Does pch_hldev_end(), setting UnitCheck in the device status
 *  and EquipmentCheck in the sense.
 */
static inline void pch_hldev_end_equipment_check(pch_devib_t *devib, uint8_t code) {
        pch_hldev_end(devib, 0, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_EQUIPMENT_CHECK,
                .code = code
        }));
}

/*! \brief Ends the current channel program, acknowledging
 *  a Halt signal from the CSS.
 *  \ingroup picochan_hldev
 *
 *  Does pch_hldev_end(), passing a normal device status and
 *  setting a sense with the Cancel flag set.
 */
static inline void pch_hldev_end_stopped(pch_devib_t *devib) {
        pch_hldev_end(devib, 0, ((pch_dev_sense_t){
                .flags = PCH_DEV_SENSE_CANCEL
        }));
}

/*! \brief Initialises hldev API use for a range of devices on a CU.
 *  \ingroup picochan_hldev
 *
 * After filling in get_hldev and start (and, optionally, signal) in
 * hdcfg, call pch_hldev_config_init() to register for the hldev API
 * the range of num_devices on CU cu starting with unit address
 * first_ua. After calling this function, channel programs started
 * from the CSS which address a devib belonging to hdcfg cause:
 * * hldev to look up the device's pch_hldev_t by calling your
 *   hdcfg->start function.
 * * (re)sets the pch_hldev_t so that
 *   - its callback is your hdcfg->start function
 *   - its ccwcmd is the CCW command
 * * calls your start callback to begin processing.
 *
 * Your processing can use the pch_hldev_receive() family functions
 * zero or more times (for a Write-type CCW) to receive data or
 * the pch_hldev_send() family functions zero or more times (for a
 * Read-Type CCW) to send data. When your processing has finished
 * (whether or not you have received/sent all data available), you
 * call one of the pch_hldev_end() family functions to end the
 * channel program. This then resets the pch_hldev_t ready to
 * start a new channel program for the device.
 *
 * The underlying CSS and CU support having a device at
 * channel-program-end time advertising a buffer that the CSS can
 * use to write data to immediately during a start of a Write-type
 * CCW but hldev does not yet provide an API for that.
 */
void pch_hldev_config_init(pch_hldev_config_t *hdcfg, pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices);

#endif
