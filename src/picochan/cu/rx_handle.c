/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "cu_internal.h"
#include "picochan/ccw.h"
#include "callback.h"
#include "cus_trace.h"

static void handle_rx_chop_data(pch_cu_t *cu, pch_devib_t *devib, proto_packet_t p) {
        pch_unit_addr_t ua = pch_get_ua(cu, devib);
	assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
	assert(devib->flags & PCH_DEVIB_FLAG_RX_DATA_REQUIRED);
	uint32_t dstaddr = devib->addr;
	uint32_t count = (uint32_t)proto_get_count(p);
        if (proto_chop_flags(p.chop) & PROTO_CHOP_FLAG_SKIP) {
                dmachan_start_dst_data_src_zeroes(&cu->rx_channel,
                        dstaddr, count);
	} else {
                dmachan_start_dst_data(&cu->rx_channel,
                        dstaddr, count);
        }

	cu->rx_active = (int16_t)ua;
}

static void handle_rx_chop_room(pch_cu_t *cu, pch_devib_t *devib, proto_packet_t p) {
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        devib->size = proto_get_count(p);
        dmachan_start_dst_cmdbuf(&cu->rx_channel);
	callback_devib(cu, devib);
}

static void handle_rx_chop_start(pch_cu_t *cu, pch_devib_t *devib, proto_packet_t p) {
        assert(!(devib->flags & PCH_DEVIB_FLAG_STARTED));
        devib->flags |= PCH_DEVIB_FLAG_STARTED;
        uint8_t ccwcmd = p.p0;
        uint16_t count = proto_decode_esize_payload(p);

	if (pch_is_ccw_cmd_write(ccwcmd)) {
		devib->flags |= PCH_DEVIB_FLAG_CMD_WRITE;
		if (count > 0) {
                        assert(count <= devib->size);
                        devib->flags |= PCH_DEVIB_FLAG_RX_DATA_REQUIRED;
                        uint32_t dstaddr = devib->addr;
			dmachan_start_dst_data(&cu->rx_channel,
			        dstaddr, (uint32_t)count);
                        cu->rx_active = (int16_t)pch_get_ua(cu, devib);
			// rx completion of incoming data will do Start callback
			return;
		}
	} else {
		// CCW command is Read-type (as seen from CSS).
		devib->flags &= ~PCH_DEVIB_FLAG_CMD_WRITE;
		devib->size = count; // advertised window we can write to
	}

        dmachan_start_dst_cmdbuf(&cu->rx_channel);
        callback_devib(cu, devib);
}

static void handle_rx_command_complete(pch_cu_t *cu) {
	// DMA has received a command packet from CSS into RxBuf
        proto_packet_t p = get_rx_packet(cu);
        pch_unit_addr_t ua = p.unit_addr;
	assert(ua < NUM_DEVIBS);
        pch_devib_t *devib = pch_get_devib(cu, ua);
	trace_dev_packet(PCH_TRC_RT_CUS_RX_COMMAND_COMPLETE, cu, devib, p);
        devib->op = p.chop;
        devib->payload = proto_get_payload(p);
	switch (proto_chop_cmd(p.chop)) {
	case PROTO_CHOP_START:
		handle_rx_chop_start(cu, devib, p);
                break;

	case PROTO_CHOP_DATA:
		handle_rx_chop_data(cu, devib, p);
                break;

	case PROTO_CHOP_ROOM:
		handle_rx_chop_room(cu, devib, p);
                break;

	default:
		panic("unexpected operation from CSS");
                // NOTREACHED
	}
}

static void handle_rx_data_complete(pch_cu_t *cu, pch_unit_addr_t ua) {
	cu->rx_active = -1;
        dmachan_start_dst_cmdbuf(&cu->rx_channel);
        pch_devib_t *devib = pch_get_devib(cu, ua);
	trace_dev(PCH_TRC_RT_CUS_RX_DATA_COMPLETE, cu, devib);
        assert(devib->flags & PCH_DEVIB_FLAG_RX_DATA_REQUIRED);
	devib->flags &= ~PCH_DEVIB_FLAG_RX_DATA_REQUIRED;
	callback_devib(cu, devib);
}

void __time_critical_func(cus_handle_rx_complete)(pch_cu_t *cu) {
        int16_t rx_active = cu->rx_active;
	if (rx_active >= 0) {
		pch_unit_addr_t ua = (pch_unit_addr_t)rx_active;
		handle_rx_data_complete(cu, ua);
	} else {
		handle_rx_command_complete(cu);
	}
}

