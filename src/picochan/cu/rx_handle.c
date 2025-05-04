/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "cu_internal.h"
#include "picochan/ccw.h"
#include "callback.h"
#include "cus_trace.h"

static void cus_handle_rx_chop_data(pch_cu_t *cu, pch_devib_t *devib, proto_packet_t p) {
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

static void cus_handle_rx_chop_room(pch_cu_t *cu, pch_devib_t *devib, proto_packet_t p) {
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        devib->size = proto_get_count(p);
        dmachan_start_dst_cmdbuf(&cu->rx_channel);
	callback_devib(cu, devib);
}

static void cus_handle_rx_chop_start_read_sense(pch_cu_t *cu, pch_devib_t *devib, pch_unit_addr_t ua, uint16_t count) {
        if (count > sizeof(devib->sense))
                count = sizeof(devib->sense);

        int rc = pch_dev_send_final(cu, ua, &devib->sense, count);
        assert(rc >= 0);
}

static void cus_handle_rx_chop_start_read_reserved(pch_cu_t *cu, pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        pch_unit_addr_t ua = pch_get_ua(cu, devib);

        switch (ccwcmd) {
        case PCH_CCW_CMD_SENSE:
                cus_handle_rx_chop_start_read_sense(cu, devib, ua,
                        count);
                break;

        default:
                pch_dev_sense_t sense = {
                        .flags = PCH_DEV_SENSE_COMMAND_REJECT
                };
                pch_dev_update_status_error(cu, ua, sense);
                break;
        }
}

static void cus_handle_rx_chop_start_read(pch_cu_t *cu, pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        devib->flags &= ~PCH_DEVIB_FLAG_CMD_WRITE;
        devib->size = count; // advertised window we can write to

        dmachan_start_dst_cmdbuf(&cu->rx_channel);

        if (ccwcmd >= PCH_CCW_CMD_FIRST_RESERVED)
                cus_handle_rx_chop_start_read_reserved(cu, devib,
                        ccwcmd, count);
        else
                callback_devib(cu, devib);
}

static void cus_handle_rx_chop_start_write(pch_cu_t *cu, pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        devib->flags |= PCH_DEVIB_FLAG_CMD_WRITE;

        if (count == 0) {
                dmachan_start_dst_cmdbuf(&cu->rx_channel);
                callback_devib(cu, devib);
                return;
        }

        assert(count <= devib->size);
        devib->flags |= PCH_DEVIB_FLAG_RX_DATA_REQUIRED;
        dmachan_start_dst_data(&cu->rx_channel,
                devib->addr, (uint32_t)count);
        cu->rx_active = (int16_t)pch_get_ua(cu, devib);
        // rx completion of incoming data will do Start callback
}

static void cus_handle_rx_chop_start(pch_cu_t *cu, pch_devib_t *devib, proto_packet_t p) {
        assert(!(devib->flags & PCH_DEVIB_FLAG_STARTED));
        devib->flags |= PCH_DEVIB_FLAG_STARTED;
        uint8_t ccwcmd = p.p0;
        uint16_t count = proto_decode_esize_payload(p);

	if (pch_is_ccw_cmd_write(ccwcmd)) {
                cus_handle_rx_chop_start_write(cu, devib, ccwcmd,
                        count);
        } else {
                cus_handle_rx_chop_start_read(cu, devib, ccwcmd,
                        count);
        }
}

static void cus_handle_rx_command_complete(pch_cu_t *cu) {
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
		cus_handle_rx_chop_start(cu, devib, p);
                break;

	case PROTO_CHOP_DATA:
		cus_handle_rx_chop_data(cu, devib, p);
                break;

	case PROTO_CHOP_ROOM:
		cus_handle_rx_chop_room(cu, devib, p);
                break;

	default:
		panic("unexpected operation from CSS");
                // NOTREACHED
	}
}

static void cus_handle_rx_data_complete(pch_cu_t *cu, pch_unit_addr_t ua) {
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
		cus_handle_rx_data_complete(cu, ua);
	} else {
		cus_handle_rx_command_complete(cu);
	}
}

