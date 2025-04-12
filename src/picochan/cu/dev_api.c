/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "cu_internal.h"
#include "cus_trace.h"

void __time_critical_func(cus_send_command_to_css)(pch_cu_t *cu) {
        int16_t tx_head = cu->tx_head;
        assert(tx_head >= 0);
        pch_unit_addr_t ua = (pch_unit_addr_t)tx_head;
        proto_packet_t p = cus_make_packet(cu, ua);
        memcpy(cu->tx_channel.cmdbuf, &p, sizeof p);
        dmachan_start_src_cmdbuf(&cu->tx_channel);
}

// Low-level "pch_devib_" API for dev implementations. These take a
// devib and simply update its fields.

// pch_devib_prepare_update_status prepares to send an UpdateStatus
// command. If it's either an unsolicited status (neither ChannelEnd
// nor DeviceEnd set) or it's end-of-channel-program (both ChannelEnd
// and DeviceEnd set) then it also sets the devib Addr and Size
// fields to dstaddr and size respectively to advertise to the CSS
// the buffer and length to which the next CCW Write-type command can
// immediately send data during Start. The window advertised will
// be the bsize encoding of Size so the actual window that the CSS
// may use will be less than Size if Size is not one of the sizes
// that bsize.Encode can encode exactly.
void __time_critical_func(pch_devib_prepare_update_status)(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size) {
        // If the channel program has started, require ChannelEnd to
        // be in devs, otherwise require it *not* to be in devs.
        assert((devib->flags & PCH_DEVIB_FLAG_STARTED)
                ^ (devs & PCH_DEVS_CHANNEL_END));

	pch_bsize_t esize = PCH_BSIZE_ZERO;
	if ((devs & PCH_DEVS_DEVICE_END) || !(devs & PCH_DEVS_CHANNEL_END)) {
		// unsolicited or end-of-channel-program: advertise window
		esize = pch_bsize_encode(size);
		devib->addr = (uint32_t)dstaddr;
		devib->size = size;
	}

	devib->op = PROTO_CHOP_UPDATE_STATUS;
	devib->payload = proto_make_devstatus_payload(devs, esize);
}

void __time_critical_func(pch_devib_send_or_queue_command)(pch_cu_t *cu, pch_unit_addr_t ua) {
        int16_t tx_tail = push_tx_list(cu, ua);
	if (tx_tail == -1) {
		// List was empty
		cus_send_command_to_css(cu);
	} else {
		trace_dev_byte(PCH_TRC_RT_CUS_QUEUE_COMMAND,
			cu, pch_get_devib(cu, ua), (uint8_t)tx_tail);
	}
}

// (Slightly) higher-level "pch_dev_" API for dev implementations.
// These take a (cu, ua) pair, update the fields of the corresponding
// devib (with "pch_devib_" functions) then call
// pch_devib_send_or_queue_command to send the command to the CSS
// either immediately (if the CU tx is available) or queue it up to
// be sent after in-progress sends.

static int set_callback(pch_devib_t *devib, uint cbindex) {
        if (!pch_cbindex_is_callable(cbindex))
                return -EINVALIDCALLBACK;

        devib->cbindex = (pch_cbindex_t)cbindex;
        return 0;
}

int __time_critical_func(pch_dev_set_callback)(pch_cu_t *cu, pch_unit_addr_t ua, int cbindex_opt) {
        if (cbindex_opt < 0)
                return 0;

        pch_devib_t *devib = pch_get_devib(cu, ua);
        return set_callback(devib, (uint)cbindex_opt);
}

int __time_critical_func(pch_dev_send_then)(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, proto_chop_flags_t flags, int cbindex_opt) {
        pch_devib_t *devib = pch_get_devib(cu, ua);
        if (!pch_devib_is_started(devib))
                return -ENOTSTARTED;

        if (pch_devib_is_cmd_write(devib))
                return -ECMDNOTREAD;

        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        // Cap write count at CSS-advertised size
        if (n > devib->size)
                n = devib->size;

        pch_devib_prepare_write_data(devib, srcaddr, n, flags);
        pch_devib_send_or_queue_command(cu, ua);
        return n;
}

int __time_critical_func(pch_dev_send_final_then)(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, int cbindex_opt) {
        return pch_dev_send_then(cu, ua, srcaddr, n,
                PROTO_CHOP_FLAG_END, cbindex_opt);
}

int __time_critical_func(pch_dev_send_final)(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n) {
        return pch_dev_send_then(cu, ua, srcaddr, n,
                PROTO_CHOP_FLAG_END, -1);
}

int __time_critical_func(pch_dev_send_respond_then)(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, int cbindex_opt) {
        return pch_dev_send_then(cu, ua, srcaddr, n,
                PROTO_CHOP_FLAG_RESPONSE_REQUIRED, cbindex_opt);
}

int __time_critical_func(pch_dev_send_respond)(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n) {
        return pch_dev_send_then(cu, ua, srcaddr, n,
                PROTO_CHOP_FLAG_RESPONSE_REQUIRED, -1);
}

int __time_critical_func(pch_dev_send_norespond_then)(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, int cbindex_opt) {
        return pch_dev_send_then(cu, ua, srcaddr, n, 0, cbindex_opt);
}

int __time_critical_func(pch_dev_send_norespond)(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n) {
        return pch_dev_send_then(cu, ua, srcaddr, n, 0, -1);
}

int __time_critical_func(pch_dev_send)(pch_cu_t *cu, pch_unit_addr_t ua, void *srcaddr, uint16_t n, proto_chop_flags_t flags) {
        return pch_dev_send_then(cu, ua, srcaddr, n, flags, -1);
}

int __time_critical_func(pch_dev_receive_then)(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size, int cbindex_opt) {
        pch_devib_t *devib = pch_get_devib(cu, ua);
        if (!pch_devib_is_started(devib))
                return -ENOTSTARTED;

        if (!pch_devib_is_cmd_write(devib))
                return -ECMDNOTWRITE;

        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        pch_devib_prepare_read_data(devib, dstaddr, size);
        pch_devib_send_or_queue_command(cu, ua);
        return 0;
}

int __time_critical_func(pch_dev_receive)(pch_cu_t *cu, pch_unit_addr_t ua, void *dstaddr, uint16_t size) {
        return pch_dev_receive_then(cu, ua, dstaddr, size, -1);
}

int __time_critical_func(pch_dev_update_status_advert_then)(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, void *dstaddr, uint16_t size, int cbindex_opt) {
        pch_devib_t *devib = pch_get_devib(cu, ua);
        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        if (!((devib->flags & PCH_DEVIB_FLAG_STARTED)
                ^ (devs & PCH_DEVS_CHANNEL_END))) {
                return -EINVALIDSTATUS;
        }

        pch_devib_prepare_update_status(devib, devs, dstaddr, size);
        pch_devib_send_or_queue_command(cu, ua);
        return 0;
}

int __time_critical_func(pch_dev_update_status_advert)(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, void *dstaddr, uint16_t size) {
        return pch_dev_update_status_advert_then(cu, ua, devs, dstaddr, size, -1);
}

int __time_critical_func(pch_dev_update_status_then)(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs, int cbindex_opt) {
        return pch_dev_update_status_advert_then(cu, ua, devs, NULL, 0, cbindex_opt);
}

int __time_critical_func(pch_dev_update_status)(pch_cu_t *cu, pch_unit_addr_t ua, uint8_t devs) {
        return pch_dev_update_status_advert_then(cu, ua, devs, NULL, 0, -1);
}

int __time_critical_func(pch_dev_send_zeroes_then)(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, proto_chop_flags_t flags, int cbindex_opt) {
        pch_devib_t *devib = pch_get_devib(cu, ua);
        if (!pch_devib_is_started(devib))
                return -ENOTSTARTED;

        if (pch_devib_is_cmd_write(devib))
                return -ECMDNOTREAD;

        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        pch_devib_prepare_write_zeroes(devib, n, flags);
        pch_devib_send_or_queue_command(cu, ua);
        return 0;
}

int __time_critical_func(pch_dev_send_zeroes)(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, proto_chop_flags_t flags) {
        return pch_dev_send_zeroes_then(cu, ua, n, flags, -1);
}

int __time_critical_func(pch_dev_send_zeroes_respond_then)(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, int cbindex_opt) {
        return pch_dev_send_zeroes_then(cu, ua, n,
                PROTO_CHOP_FLAG_RESPONSE_REQUIRED, cbindex_opt);
}

int __time_critical_func(pch_dev_send_zeroes_respond)(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n) {
        return pch_dev_send_zeroes_then(cu, ua, n,
                PROTO_CHOP_FLAG_RESPONSE_REQUIRED, -1);
}

int __time_critical_func(pch_dev_send_zeroes_norespond_then)(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n, int cbindex_opt) {
        return pch_dev_send_zeroes_then(cu, ua, n, 0, cbindex_opt);
}

int __time_critical_func(pch_dev_send_zeroes_norespond)(pch_cu_t *cu, pch_unit_addr_t ua, uint16_t n) {
        return pch_dev_send_zeroes_then(cu, ua, n, 0, -1);
}
