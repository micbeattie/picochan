/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "cu_internal.h"
#include "picochan/dev_status.h"
#include "cus_trace.h"

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
        bool callback_pending = !proto_chop_has_response_required(op)
                && !proto_chop_has_end(op);
        pch_devib_set_callback_pending(devib, callback_pending);

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

void __time_critical_func(pch_cus_handle_tx_complete)(pch_cu_t *cu) {
	pch_txsm_t *txpend = &cu->tx_pending;
        pch_devib_t *devib = pch_cu_head_devib(cu, &cu->tx_list);
        assert(devib);

        // Poison TxBuf to help troubleshooting
        cu->channel.tx.link.cmd.raw = 0xffffffff;

        bool callback_pending = pch_devib_is_callback_pending(devib);
	trace_tx_complete(PCH_TRC_RT_CUS_TX_COMPLETE, cu,
                pch_dev_get_ua(devib),
                callback_pending, txpend->state);

        pch_txsm_run_result_t res = pch_txsm_run(txpend, &cu->channel.tx);
        if (res == PCH_TXSM_ACTED)
                return;

        pch_cu_pop_devib(cu, &cu->tx_list);
        pch_devib_set_tx_busy(devib, false);
        if (callback_pending) {
                pch_devib_set_callback_pending(devib, false);
                pch_cu_push_devib(cu, &cu->cb_list, devib);
                pch_cu_schedule_worker(cu);
        }
}

void __no_inline_not_in_flash_func(pch_cu_send_pending_tx_command)(pch_cu_t *cu, pch_devib_t *devib) {
        pch_devib_set_tx_busy(devib, true);
        proto_packet_t p = pch_cus_make_packet(devib);
        uint32_t cmd = proto_packet_as_word(p);
        dmachan_link_t *txl = &cu->channel.tx.link;
        dmachan_link_cmd_set(txl, dmachan_make_cmd_from_word(cmd));
        trace_dev_packet(PCH_TRC_RT_CUS_SEND_TX_PACKET, devib, p,
                dmachan_link_seqnum(txl));
        dmachan_start_src_cmdbuf(&cu->channel.tx);
}
