/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CU_DEV_API_H
#define _PCH_CU_DEV_API_H

#include "picochan/devib.h"

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
// that simply specialise the callback and flags fields

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
};

// dev API with fully general arguments
int pch_dev_set_callback(pch_cu_t *cu, pch_unit_addr_t ua, int cbindex_opt);
int pch_dev_send_then(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);
int pch_dev_send_zeroes_then(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, proto_chop_flags_t flags, int cbindex_opt);
int pch_dev_receive_then(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size, int cbindex_opt);
int pch_dev_update_status_advert_then(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, void *dstaddr, uint16_t size, int cbindex_opt);

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

#endif
