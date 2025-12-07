/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CU_DEV_API_H
#define _PCH_CU_DEV_API_H

#include "picochan/devib.h"

/*! \file picochan/dev_api.h
 *  \ingroup picochan_cu
 *
 * \brief The main API for a device on a CU
 *
 * These provide a slightly higher-level API by wrapping the
 * low-level pch_devib_ API functions.
 */

// Main API for dev implementation, slightly higher level than
// devib ones. They return negative error values on error
// (e.g. -EINVAL). They do various parameter checks and return
// errors instead of asserting like the low-level API does. Those
// with cbindex_opt arguments leave the devib cbindex field alone
// if called with a negative value, otherwise they validate it
// as a callback cbindex and set the field or return a negative
// error value, as appropriate.  For sends (of data or zeroes), the
// length sent is validated to be under the CSS-advertised window
// (devib->size) and capped at that if not, with the actual count
// returned. Many functions are variants of the full generic ones
// that simply specialise the callback and flags fields.
// Values between 1 and 255 are typically used to fit into the ASC
// byte of a pch_dev_sense_t with sense code
// PCH_DEV_SENSE_COMMAND_REJECT. ECANCEL is associated with sense
// code PCH_DEV_SENSE_CANCEL.

enum {
        ENOSUCHERROR	        = 1,
        EINVALIDCALLBACK	= 2,
        ENOTSTARTED		= 3,
        ECMDNOTREAD		= 4,
        ECMDNOTWRITE		= 5,
        EWRITETOOBIG		= 6,
        EINVALIDSTATUS		= 7,
        EINVALIDDEV             = 8,
        EINVALIDCMD             = 9,
        EINVALIDVALUE           = 10,
        EDATALENZERO            = 11,
        EBUFFERTOOSHORT         = 12,
        ECUBUSY                 = 13,
        //
        ECANCEL                 = 256
};

// dev API with fully general arguments

/*! \brief Set callback for device
 *  \ingroup picochan_cu
 *
 * Sets, changes or unsets the callback function that the CU invokes
 * when action is needed from the device.
 * \param cu the CU to which the device belongs
 * \param ua the unit address of the device within its CU
 * \param cbindex_opt either a callback index (pch_devib_callback_t) of a
 * callback function registered with pch_register_devib_callback or
 * one of the following special values:
 *  * PCH_DEVIB_CALLBACK_DEFAULT - any attempt by the CSS to start a
 *  channel program for this device will result in the CU responding
 *  on its behalf with a final device status (ChannelEnd|DeviceEnd)
 *  with UnitCheck set and a sense code set with CommandReject with
 *  additional code EINVALIDDEV. Any attempt to callback the device
 *  at any other point in its lifecycle will result in the CU
 *  responding on its behalf with a final device status
 *  (ChannelEnd|DeviceEnd) with UnitCheck set and a sense code set
 *  with ProtoError, an additional code of the requested operation
 *  and ASC and ASCQ containing the bytes p0 and p1, respectively,
 *  of the operation packet payload.
 *  * PCH_DEVIB_CALLBACK_NOOP - any attempt to callback this device
 *  will be silently ignored. For this to be at all useful, the device
 *  must be specially written to determine any actions needed of it
 *  independently of the usual CU-to-device communication mechanisms.
 *  * -1 - the device callback is not changed
 */
int pch_dev_set_callback(pch_devib_t *devib, int cbindex_opt);

/*! \brief Sends data to the CSS
 * \ingroup picochan_cu
 *
 * This, and related variants, is the primary function used to send
 * data to the CSS satisfying some or all of a CCW segment with a
 * Read-type command. Before calling this function, the device must
 * have verified that (1) the CSS is expecting data to be sent and
 * (2) the amount of data it sends is no more than the maximum space
 * advertised by the CSS. For (1),
 * * the Start callback must have been called for the device and the
 * device has not since sent an UpdateStatus including ChannelEnd
 * * and the CCW command must have been Read-Type (the devib->flags
 * field must have the PCH_DEVIB_FLAG_CMD_WRITE bit as zero).
 *
 * For (2), provided (1) holds, the devib->size field will have been
 * filled in at Start time with a size that is no more than (and will
 * typically be very close to) the size specified by the CCW segment
 * itself. However, the size field is not affected by using this or
 * related functions to send data to the CSS (and the field should
 * not be updated in such a way by the device). Use the
 * PROTO_CHOP_FLAG_RESPONSE_REQUIRED flag (see below) if up-to-date
 * and/or exact size information is needed.
 * \param cu - the control unit
 * \param ua - the unit address of the device in control unit `cu`
 * \param flags - may contain the following flags:
 * * PROTO_CHOP_FLAG_RESPONSE_REQUIRED -request that the CSS send an
 * update (a Room operation) that causes the CU to update the
 * `devib->size` field with up-to-date and exact information.
 * * PROTO_CHOP_FLAG_END - after sending the data, the CSS will
 * behave as though the device has sent a final device status with no
 * unusual conditions (DeviceEnd|ChannelEnd and no other bits set).
 * * PROTO_CHOP_FLAG_SKIP - instead of sending n data bytes down the
 * channel, the CSS will behave as though n bytes of zeroes were
 * sent. If this flag is set, srcaddr is ignored.
 *
 * \param srcaddr - the address of the data to be sent (ignored if
 * flags contains PROTO_CHOP_FLAG_SKIP)
 * \param n - the number of data bytes to send
 * \param cbindex_opt - before sending, update the callback index
 * in the devib (unless -1 is passed) ready for the next callback to
 * the device. The event that will cause the next callback depends on
 * the flags:
 * * PROTO_CHOP_FLAG_RESPONSE_REQUIRED - the callback will happen
 * after the CSS has replied with its Room operation and the CU has
 * updated the `devib->size` field with an up-to-date and exact size.
 * * PROTO_CHOP_FLAG_END - the next callback will be when the next
 * CCW is processed causing a Start to the device (whether a CCW
 * command-chained from the previous channel program or a new channel
 * program - the difference is not visible to the device).
 * * any other combination - the callback will happen as soon as the
 * CU has completed sending the command+data to the CSS meaning that
 * the device can invoke further API calls if it wishes. Whether any
 * new API calls will cause commands to be sent to the CSS
 * immediately depends on whether any other devices have commands
 * that are being sent or are pending ahead of new requests from
 * this device.
 */
int pch_dev_send_then(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);

/*! \brief Sends zeroes to the CSS
 *  \ingroup picochan_cu
 *
 * Convenience function that calls pch_dev_send_then with a flags
 * field that ORs in PROTO_CHOP_FLAG_SKIP and an (ignored) srcaddr
 * of 0.
 */
int pch_dev_send_zeroes_then(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);

/*! \brief Receive data from the CSS
 *  \ingroup picochan_cu
 *
 * This, and related variants, is the primary function used to receive
 * data from the CSS from the source address and count specified in a
 * CCW segment with a Write-type command. Before calling this
 * function, the device must have verified that the CSS is
 * expecting to send data, i.e.
 * * the Start callback must have been called for the device and the
 * device has not since sent an UpdateStatus including ChannelEnd
 * * and the CCW command must have been Write-Type (the devib->flags
 * field must have the PCH_DEVIB_FLAG_CMD_WRITE bit set).
 *
 * If the device requests more data than the CCW segment contains
 * then the amount of data sent to the device will be safely capped
 * at the available amount but additional effects depend on flags
 * set in the CCW and, possibly, the subchannel. A request by the
 * device for more data than is available is an
 * "Incorrect Length Condition" and, unless the channel program has
 * included the PCH_CCW_FLAG_SLI ("Suppress Length Indication") flag
 * in the CCW, will cause the channel program to stop any data
 * chaining or command chaining and end (eventually) with a
 * subchannel status field including the PCH_SCHS_INCORRECT_LENGTH
 * flag. It is up to the device driver author to be aware of the
 * effects the request counts may have on the channel program and,
 * ideally, use them and document them in a way that allows the
 * channel program author to construct channel programs that can
 * make good use of the additional length checks or have them
 * ignored where appropriate.
 *
 * The `devib->size` field will have been filled in at Start time with
 * a size that is no more than (and will typically be very close to)
 * the size specified by the CCW segment itself. Following a call to
 * `pch_dev_receive_then()` or its variants, the response from the CSS
 * includes an exact up-to-date count of the remaining available
 * room in the CCW segment and the CU updates the `devib->size` field
 * with this value before invoking the next callback on the device.
 * \param cu - the control unit
 * \param ua - the unit address of the device in control unit `cu`
 * \param dstaddr - the address to receive the data sent by the CSS
 * \param size - the number of data bytes requested - the number of
 * bytes actually received will be at most `n` but may be
 * strictly less.
 * \param cbindex_opt - before sending, update the callback index
 * in the devib (unless -1 is passed) ready for the next callback to
 * the device, which will happen after the data has been received
 * and the CU has updated the `devib->size` field with the
 * remaining count of available data bytes.
 */
int pch_dev_receive_then(pch_devib_t *devib, void *dstaddr, uint16_t size, int cbindex_opt);

int pch_dev_update_status_advert_then(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size, int cbindex_opt);

// dev API convenience functions with some fixed arguments:
// * Omitting _then avoids setting devib callback by hardcoding -1
// as the cbindex_opt argument of the full _then function.
// * For send and send_zeroes family, the flags argument is set to
//     * PROTO_CHOP_FLAG_END for the _final variant,
//     * PROTO_CHOP_FLAG_RESPONSE_REQUIRED for the _respond variant
//     * 0 for the _norespond variant
// * For pch_dev_update_status_ok family, call the corresponding
// pch_dev_update_status_ function with DeviceEnd|ChannelEnd
// * For pch_dev_update_status_error family, set devib->sense to the
// sense argument then call the corresponding pch_dev_update_status_
// function with a device status of DeviceEnd|ChannelEnd|UnitCheck
int pch_dev_send(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags);
int pch_dev_send_final(pch_devib_t *devib, void *srcaddr, uint16_t n);
int pch_dev_send_final_then(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_respond(pch_devib_t *devib, void *srcaddr, uint16_t n);
int pch_dev_send_respond_then(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_norespond(pch_devib_t *devib, void *srcaddr, uint16_t n);
int pch_dev_send_norespond_then(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags);
int pch_dev_send_zeroes_respond_then(pch_devib_t *devib, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes_respond(pch_devib_t *devib, uint16_t n);
int pch_dev_send_zeroes_norespond_then(pch_devib_t *devib, uint16_t n, int cbindex_opt);
int pch_dev_send_zeroes_norespond(pch_devib_t *devib, uint16_t n);
int pch_dev_receive(pch_devib_t *devib, void *dstaddr, uint16_t size);
int pch_dev_update_status_then(pch_devib_t *devib, uint8_t devs, int cbindex_opt);
int pch_dev_update_status(pch_devib_t *devib, uint8_t devs);
int pch_dev_update_status_advert(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size);
int pch_dev_update_status_ok_then(pch_devib_t *devib, int cbindex_opt);
int pch_dev_update_status_ok(pch_devib_t *devib);
int pch_dev_update_status_ok_advert(pch_devib_t *devib, void *dstaddr, uint16_t size);
int pch_dev_update_status_error_advert_then(pch_devib_t *devib, pch_dev_sense_t sense, void *dstaddr, uint16_t size, int cbindex_opt);
int pch_dev_update_status_error_then(pch_devib_t *devib, pch_dev_sense_t sense, int cbindex_opt);
int pch_dev_update_status_error_advert(pch_devib_t *devib, pch_dev_sense_t sense, void *dstaddr, uint16_t size);
int pch_dev_update_status_error(pch_devib_t *devib, pch_dev_sense_t sense);

typedef int (*pch_dev_call_func_t)(pch_devib_t *devib);

/*! Calls f and, if it returns a negative value, sets an appropriate
 * sense, triggers an UpdateStatus to report the error and sets the
 * "next callback" index. If f returns a non-negative value, no
 * action is taken. In either case, the return value of f is
 * propagated to the caller.
 *
 * When f returns a negative value between -1 and -255, the sense set
 * is CommandReject with an ASC byte of the associated negated
 * (positive) error value. When f returns -ECANCEL (-256), the
 * sense set is Cancel.
 */
int pch_dev_call_or_reject_then(pch_devib_t *devib, pch_dev_call_func_t f, int reject_cbindex_opt);

/*! Calls f, sends an UpdateStatus with an appropriate payload based
 * on its return value then sets cbindex_opt as the next callback.
 * If f returns a negative value, the UpdateStatus payload is
 * UnitCheck with sense CommandReject with the associated negated
 * (positive) error value or else, if f returns a non-negative
 * valuem the UpdateStatus payload is normal "no error" with
 * ChannelEnd|DeviceEnd.
 */
void pch_dev_call_final_then(pch_devib_t *devib, pch_dev_call_func_t f, int cbindex_opt);

#endif
