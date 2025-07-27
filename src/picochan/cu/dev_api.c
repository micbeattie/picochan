/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "cu_internal.h"
#include "cus_trace.h"

// push_tx_list pushes ua onto the singly-linked list with head and
// tail cu->tx_head and cu->tx_tail and returns the old tail.
// All manipulation is done under the devibs_lock.
static int16_t __no_inline_not_in_flash_func(push_tx_list)(pch_cu_t *cu, pch_unit_addr_t ua) {
        uint32_t status = devibs_lock();
        int16_t tx_tail = cu->tx_tail;
        if (tx_tail < 0) {
                cu->tx_head = (uint16_t)ua;
                cu->tx_tail = (uint16_t)ua;
        } else {
                // There's already a pending list: add ourselves at the end
                pch_unit_addr_t tx_tail_ua = (pch_unit_addr_t)tx_tail;
                pch_devib_t *tx_tail_devib = pch_get_devib(cu, tx_tail_ua);
                tx_tail_devib->next = ua;
                cu->tx_tail = (int16_t)ua;
        }

        devibs_unlock(status);
        return tx_tail;
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
void __no_inline_not_in_flash_func(pch_devib_prepare_update_status)(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size) {
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

void __no_inline_not_in_flash_func(pch_devib_send_or_queue_command)(pch_devib_t *devib) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        int16_t tx_tail = push_tx_list(cu, ua);
	if (tx_tail == -1) {
		// List was empty
		cus_send_command_to_css(cu);
                // Process any resulting immediate tx completions
                dmachan_link_t *txl = &cu->tx_channel.link;
                while (txl->complete) {
                        txl->complete = false;
                        cus_handle_tx_complete(cu);
                }
	} else {
		trace_dev_byte(PCH_TRC_RT_CUS_QUEUE_COMMAND,
			devib, (uint8_t)tx_tail);
	}
}

// (Slightly) higher-level "pch_dev_" API for dev implementations.
// These update the fields of the corresponding devib
// (with "pch_devib_" functions) then call
// pch_devib_send_or_queue_command to send the command to the CSS
// either immediately (if the CU tx is available) or queue it up to
// be sent after in-progress sends.

static int set_callback(pch_devib_t *devib, int cbindex_opt) {
        if (cbindex_opt < 0)
                return 0;

        uint cbindex = (uint)cbindex_opt;
        if (!pch_cbindex_is_callable(cbindex))
                return -EINVALIDCALLBACK;

        devib->cbindex = (pch_cbindex_t)cbindex;
        return 0;
}

int __time_critical_func(pch_dev_set_callback)(pch_devib_t *devib, int cbindex_opt) {
        if (cbindex_opt < 0)
                return 0;

        return set_callback(devib, cbindex_opt);
}

int __no_inline_not_in_flash_func(pch_dev_send_then)(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags, int cbindex_opt) {
        if (!pch_devib_is_started(devib))
                return -ENOTSTARTED;

        if (pch_devib_is_cmd_write(devib))
                return -ECMDNOTREAD;

        if (n == 0)
                return -EDATALENZERO;

        int err = pch_dev_set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        // Cap write count at CSS-advertised size
        if (n > devib->size)
                n = devib->size;

        pch_devib_prepare_write_data(devib, srcaddr, n, flags);
        pch_devib_send_or_queue_command(devib);
        return n;
}

int __time_critical_func(pch_dev_send_final_then)(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt) {
        return pch_dev_send_then(devib, srcaddr, n,
                PROTO_CHOP_FLAG_END, cbindex_opt);
}

int __time_critical_func(pch_dev_send_final)(pch_devib_t *devib, void *srcaddr, uint16_t n) {
        return pch_dev_send_then(devib, srcaddr, n,
                PROTO_CHOP_FLAG_END, -1);
}

int __time_critical_func(pch_dev_send_respond_then)(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt) {
        return pch_dev_send_then(devib, srcaddr, n,
                PROTO_CHOP_FLAG_RESPONSE_REQUIRED, cbindex_opt);
}

int __time_critical_func(pch_dev_send_respond)(pch_devib_t *devib, void *srcaddr, uint16_t n) {
        return pch_dev_send_then(devib, srcaddr, n,
                PROTO_CHOP_FLAG_RESPONSE_REQUIRED, -1);
}

int __time_critical_func(pch_dev_send_norespond_then)(pch_devib_t *devib, void *srcaddr, uint16_t n, int cbindex_opt) {
        return pch_dev_send_then(devib, srcaddr, n, 0, cbindex_opt);
}

int __time_critical_func(pch_dev_send_norespond)(pch_devib_t *devib, void *srcaddr, uint16_t n) {
        return pch_dev_send_then(devib, srcaddr, n, 0, -1);
}

int __time_critical_func(pch_dev_send)(pch_devib_t *devib, void *srcaddr, uint16_t n, proto_chop_flags_t flags) {
        return pch_dev_send_then(devib, srcaddr, n, flags, -1);
}

int __no_inline_not_in_flash_func(pch_dev_receive_then)(pch_devib_t *devib, void *dstaddr, uint16_t size, int cbindex_opt) {
        if (!pch_devib_is_started(devib))
                return -ENOTSTARTED;

        if (!pch_devib_is_cmd_write(devib))
                return -ECMDNOTWRITE;

        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        pch_devib_prepare_read_data(devib, dstaddr, size);
        pch_devib_send_or_queue_command(devib);
        return 0;
}

int __time_critical_func(pch_dev_receive)(pch_devib_t *devib, void *dstaddr, uint16_t size) {
        return pch_dev_receive_then(devib, dstaddr, size, -1);
}

int __no_inline_not_in_flash_func(pch_dev_update_status_advert_then)(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size, int cbindex_opt) {
        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        if (!((devib->flags & PCH_DEVIB_FLAG_STARTED)
                ^ (devs & PCH_DEVS_CHANNEL_END))) {
                return -EINVALIDSTATUS;
        }

        pch_devib_prepare_update_status(devib, devs, dstaddr, size);
        pch_devib_send_or_queue_command(devib);
        return 0;
}

int __time_critical_func(pch_dev_update_status_advert)(pch_devib_t *devib, uint8_t devs, void *dstaddr, uint16_t size) {
        return pch_dev_update_status_advert_then(devib, devs, dstaddr, size, -1);
}

int __time_critical_func(pch_dev_update_status_then)(pch_devib_t *devib, uint8_t devs, int cbindex_opt) {
        return pch_dev_update_status_advert_then(devib, devs, NULL, 0, cbindex_opt);
}

int __time_critical_func(pch_dev_update_status)(pch_devib_t *devib, uint8_t devs) {
        return pch_dev_update_status_advert_then(devib, devs, NULL, 0, -1);
}

int __no_inline_not_in_flash_func(pch_dev_update_status_ok_advert_then)(pch_devib_t *devib, void *dstaddr, uint16_t size, int cbindex_opt) {
        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        uint8_t devs = PCH_DEVS_CHANNEL_END | PCH_DEVS_DEVICE_END;
        pch_devib_prepare_update_status(devib, devs, dstaddr, size);
        pch_devib_send_or_queue_command(devib);
        return 0;
}

int __time_critical_func(pch_dev_update_status_ok_advert)(pch_devib_t *devib, void *dstaddr, uint16_t size) {
        return pch_dev_update_status_ok_advert_then(devib, dstaddr, size, -1);
}

int __time_critical_func(pch_dev_update_status_ok_then)(pch_devib_t *devib, int cbindex_opt) {
        return pch_dev_update_status_ok_advert_then(devib, NULL, 0, cbindex_opt);
}

int __time_critical_func(pch_dev_update_status_ok)(pch_devib_t *devib) {
        return pch_dev_update_status_ok_advert_then(devib, NULL, 0, -1);
}

int __no_inline_not_in_flash_func(pch_dev_update_status_error_advert_then)(pch_devib_t *devib, pch_dev_sense_t sense, void *dstaddr, uint16_t size, int cbindex_opt) {
        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        devib->sense = sense;
        uint8_t devs = PCH_DEVS_CHANNEL_END | PCH_DEVS_DEVICE_END
                | PCH_DEVS_UNIT_CHECK;
        pch_devib_prepare_update_status(devib, devs, dstaddr, size);
        pch_devib_send_or_queue_command(devib);
        return 0;
}

int __time_critical_func(pch_dev_update_status_error_advert)(pch_devib_t *devib, pch_dev_sense_t sense, void *dstaddr, uint16_t size) {
        return pch_dev_update_status_error_advert_then(devib, sense, dstaddr, size, -1);
}

int __time_critical_func(pch_dev_update_status_error_then)(pch_devib_t *devib, pch_dev_sense_t sense, int cbindex_opt) {
        return pch_dev_update_status_error_advert_then(devib, sense, NULL, 0, cbindex_opt);
}

int __time_critical_func(pch_dev_update_status_error)(pch_devib_t *devib, pch_dev_sense_t sense) {
        return pch_dev_update_status_error_advert_then(devib, sense, NULL, 0, -1);
}

int __time_critical_func(pch_dev_send_zeroes_then)(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags, int cbindex_opt) {
        if (!pch_devib_is_started(devib))
                return -ENOTSTARTED;

        if (pch_devib_is_cmd_write(devib))
                return -ECMDNOTREAD;

        int err = set_callback(devib, cbindex_opt);
        if (err < 0)
                return err;

        pch_devib_prepare_write_zeroes(devib, n, flags);
        pch_devib_send_or_queue_command(devib);
        return 0;
}

int __time_critical_func(pch_dev_send_zeroes)(pch_devib_t *devib, uint16_t n, proto_chop_flags_t flags) {
        return pch_dev_send_zeroes_then(devib, n, flags, -1);
}

int __time_critical_func(pch_dev_send_zeroes_respond_then)(pch_devib_t *devib, uint16_t n, int cbindex_opt) {
        return pch_dev_send_zeroes_then(devib, n,
                PROTO_CHOP_FLAG_RESPONSE_REQUIRED, cbindex_opt);
}

int __time_critical_func(pch_dev_send_zeroes_respond)(pch_devib_t *devib, uint16_t n) {
        return pch_dev_send_zeroes_then(devib, n,
                PROTO_CHOP_FLAG_RESPONSE_REQUIRED, -1);
}

int __time_critical_func(pch_dev_send_zeroes_norespond_then)(pch_devib_t *devib, uint16_t n, int cbindex_opt) {
        return pch_dev_send_zeroes_then(devib, n, 0, cbindex_opt);
}

int __time_critical_func(pch_dev_send_zeroes_norespond)(pch_devib_t *devib, uint16_t n) {
        return pch_dev_send_zeroes_then(devib, n, 0, -1);
}

void __time_critical_func(pch_dev_call_final_then)(pch_devib_t *devib, pch_dev_call_func_t f, int cbindex_opt) {
        int rc = f(devib);

        uint8_t devs = PCH_DEVS_CHANNEL_END | PCH_DEVS_DEVICE_END;
        if (rc < 0) {
                devs |= PCH_DEVS_UNIT_CHECK;
                pch_dev_sense_t sense = {
                        .flags = PCH_DEV_SENSE_COMMAND_REJECT,
                        .asc = (uint8_t)(-rc),
                };
                devib->sense = sense;
        }
        pch_dev_update_status_then(devib, devs, cbindex_opt);
}
