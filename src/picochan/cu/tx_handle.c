/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "cu_internal.h"
#include "picochan/dev_status.h"
#include "callback.h"
#include "cus_trace.h"

static inline void try_tx_next_command(pch_cu_t *cu) {
        if (cu->tx_head > -1)
                pch_cus_send_command_to_css(cu);
}

static void __no_inline_not_in_flash_func(pch_pop_tx_list)(pch_cu_t *cu) {
        int16_t current = cu->tx_head;
        assert(current != -1);
        pch_unit_addr_t ua = (pch_unit_addr_t)current;
        pch_devib_t *devib = pch_get_devib(cu, ua);

        pch_unit_addr_t next = devib->next;
        if (next == ua) {
                cu->tx_head = -1;
                cu->tx_tail = -1;
        } else {
                cu->tx_head = (int16_t)next;
                devib->next = ua; // remove from list by pointing at self
        }
}

// make_update_status verifies the prepared UpdateStatus in devib is
// valid for sending to the CSS. It then unsets the Started flag if
// the dev.Status being sent includes DeviceEnd (indicating end of
// channel program).
static void make_update_status(pch_devib_t *devib) {
        proto_payload_t p = devib->payload;
        uint8_t devs = proto_parse_devstatus_payload_devs(p);
	assert(pch_bsize_decode(proto_parse_devstatus_payload_esize(p)) <= devib->size);

	if (devs & PCH_DEVS_DEVICE_END) {
		if (devib->flags & PCH_DEVIB_FLAG_STARTED) {
                        assert(devs & PCH_DEVS_CHANNEL_END);
			devib->flags &= ~PCH_DEVIB_FLAG_STARTED;
		}
	} else if (devs & PCH_DEVS_CHANNEL_END) {
                assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
	} else {
                assert(!(devib->flags & PCH_DEVIB_FLAG_STARTED));
	}
}

static void make_data_command(pch_devib_t *devib) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        uint16_t count = proto_parse_count_payload(devib->payload);

        assert(!(devib->flags & PCH_DEVIB_FLAG_CMD_WRITE));
        assert(count > 0 && count <= devib->size);
        assert(!pch_txsm_busy(&cu->tx_pending));

	proto_chop_t op = devib->op;
        // If no response packet required and not a final auto-end
        // send then arrange for callback immediately after tx of data
	if (!proto_chop_has_response_required(op)
                && !proto_chop_has_end(op)) {
		cu->tx_callback_ua = (int16_t)pch_dev_get_ua(devib);
        } else {
		cu->tx_callback_ua = -1;
        }

        // If the End flag is set then the data we're sending has an
        // implicit following UpdateStatus with a plain
        // ChannelEnd|DeviceEnd so unset the Started flag as though
        // we'd sent an explicit one
	if (proto_chop_has_end(op))
                devib->flags &= ~PCH_DEVIB_FLAG_STARTED;

	if (!proto_chop_has_skip(op))
                pch_txsm_set_pending(&cu->tx_pending, devib->addr, count);
}

static void make_request_read(pch_devib_t *devib) {
        assert(devib->flags & PCH_DEVIB_FLAG_CMD_WRITE);
        (void)devib;
}

static proto_packet_t pch_cus_make_packet(pch_devib_t *devib) {
	proto_chop_t op = devib->op;

	switch (proto_chop_cmd(op)) {
	case PROTO_CHOP_UPDATE_STATUS:
		make_update_status(devib);
                break;

	case PROTO_CHOP_DATA:
		make_data_command(devib);
                break;

	case PROTO_CHOP_REQUEST_READ:
		make_request_read(devib);
                break;

        default:
                // nothing to do for other commands
                // fallthrough
	}

        pch_unit_addr_t ua = pch_dev_get_ua(devib);
        return proto_make_packet(op, ua, devib->payload);
}

static inline void callback_devib_from_tx(pch_devib_t *devib) {
        pch_devib_set_tx_callback(devib, false);
        callback_devib(devib);
}

void __time_critical_func(pch_cus_handle_tx_complete)(pch_cu_t *cu) {
	pch_txsm_t *txpend = &cu->tx_pending;
	int16_t tx_callback_uaopt = cu->tx_callback_ua;
	int16_t tx_head = cu->tx_head;
        assert(tx_head >= 0);
        assert(tx_callback_uaopt == -1 || tx_callback_uaopt == tx_head);
        pch_unit_addr_t ua = (pch_unit_addr_t)tx_head;
        pch_devib_t *devib = pch_get_devib(cu, ua);
        pch_devib_set_tx_busy(devib, false);

	trace_tx_complete(PCH_TRC_RT_CUS_TX_COMPLETE, cu,
                tx_callback_uaopt, txpend->state);

        pch_txsm_run_result_t res = pch_txsm_run(txpend, &cu->tx_channel);
        switch (res) {
        case PCH_TXSM_ACTED:
		return;

        case PCH_TXSM_FINISHED:
		pch_pop_tx_list(cu);
		if (tx_callback_uaopt != -1) {
                        cu->tx_callback_ua = -1;
			callback_devib_from_tx(devib);
		}
		try_tx_next_command(cu);
		return;

        default:
                // fallthrough
	}

        if (pch_devib_is_tx_callback(devib))
                callback_devib_from_tx(devib);

	pch_pop_tx_list(cu);
	try_tx_next_command(cu);
}

void __no_inline_not_in_flash_func(pch_cus_send_command_to_css)(pch_cu_t *cu) {
	int16_t tx_head = cu->tx_head;
        assert(tx_head >= 0);
	pch_unit_addr_t ua = (pch_unit_addr_t)tx_head;
        pch_devib_t *devib = pch_get_devib(cu, ua);
        proto_packet_t p = pch_cus_make_packet(devib);
        DMACHAN_LINK_CMD_COPY(&cu->tx_channel.link, &p);
        trace_dev_packet(PCH_TRC_RT_CUS_SEND_TX_PACKET, devib, p);
        dmachan_start_src_cmdbuf(&cu->tx_channel);
}
