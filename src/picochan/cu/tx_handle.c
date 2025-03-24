/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "cu_internal.h"
#include "picochan/dev_status.h"
#include "callback.h"
#include "cus_trace.h"

// make_update_status verifies the prepared UpdateStatus in devib is
// valid for sending to the CSS. It then unsets the Started flag if
// the dev.Status being sent include DeviceEnd (indicating end of
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

static void make_data_command(pch_cu_t *cu, pch_devib_t *devib) {
	pch_unit_addr_t ua = pch_get_ua(cu, devib);
        uint16_t count = proto_parse_count_payload(devib->payload);

        assert(!(devib->flags & PCH_DEVIB_FLAG_CMD_WRITE));
        assert(count <= devib->size);
        assert(!pch_txsm_busy(&cu->tx_pending));

	proto_chop_t op = devib->op;
        // If no response packet required, arrange for callback
        // immediately after tx of data
	if (!(op & PROTO_CHOP_FLAG_RESPONSE_REQUIRED))
		cu->tx_callback_ua = (int16_t)ua;
	 else
		cu->tx_callback_ua = -1;

	if (!(op & PROTO_CHOP_FLAG_SKIP))
                pch_txsm_set_pending(&cu->tx_pending, devib->addr, count);
}

static void make_request_read(pch_devib_t *devib) {
        assert(devib->flags & PCH_DEVIB_FLAG_CMD_WRITE);
}

proto_packet_t __time_critical_func(cus_make_packet)(pch_cu_t *cu, pch_unit_addr_t ua) {
        pch_devib_t *devib = pch_get_devib(cu, ua);
	proto_chop_t op = devib->op;

	switch (proto_chop_cmd(op)) {
	case PROTO_CHOP_UPDATE_STATUS:
		make_update_status(devib);
                break;

	case PROTO_CHOP_DATA:
		make_data_command(cu, devib);
                break;

	case PROTO_CHOP_REQUEST_READ:
		make_request_read(devib);
                break;

        default:
                // nothing to do for other commands
                // fallthrough
	}

        proto_packet_t p = proto_make_packet(op, ua, devib->payload);
	trace_dev_packet(PCH_TRC_RT_CUS_MAKE_PACKET, cu, devib, p);
	return p;
}

void __time_critical_func(cus_handle_tx_complete)(pch_cu_t *cu) {
	pch_txsm_t *txpend = &cu->tx_pending;
	int16_t tx_callback_uaopt = cu->tx_callback_ua;
	trace_tx_complete(PCH_TRC_RT_CUS_TX_COMPLETE, cu,
                tx_callback_uaopt, txpend->state);

        pch_txsm_run_result_t res = pch_txsm_run(txpend, &cu->tx_channel);
        switch (res) {
        case PCH_TXSM_ACTED:
		return;

        case PCH_TXSM_FINISHED:
		pop_tx_list(cu);
		if (tx_callback_uaopt != -1) {
                        pch_unit_addr_t ua = (pch_unit_addr_t)tx_callback_uaopt;
                        pch_devib_t *devib = pch_get_devib(cu, ua);
			callback_devib(cu, devib);
		}
		try_tx_next_command(cu);
		return;

        default:
                // fallthrough
	}

	int16_t tx_head = cu->tx_head;
        assert(tx_head >= 0);
	pch_unit_addr_t ua = (pch_unit_addr_t)tx_head;
        pch_devib_t *devib = pch_get_devib(cu, ua);
        if (devib->flags & PCH_DEVIB_FLAG_TX_CALLBACK)
                callback_devib(cu, devib);

	pop_tx_list(cu);
	try_tx_next_command(cu);
}
