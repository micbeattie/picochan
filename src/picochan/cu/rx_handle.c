/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "cu_internal.h"
#include "picochan/ccw.h"
#include "callback.h"
#include "cus_trace.h"

static void __not_in_flash_func(cus_handle_rx_chop_data)(pch_devib_t *devib, proto_packet_t p) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
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

static void __not_in_flash_func(cus_handle_rx_chop_room)(pch_devib_t *devib, proto_packet_t p) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
        devib->size = proto_get_count(p);
        dmachan_start_dst_cmdbuf(&cu->rx_channel);
	callback_devib(devib);
}

static void __not_in_flash_func(cus_handle_rx_chop_halt)(pch_devib_t *devib, proto_packet_t p) {
        if (!(devib->flags & PCH_DEVIB_FLAG_STARTED))
                return;

        devib->flags |= PCH_DEVIB_FLAG_STOPPING;
        callback_devib(devib);
}

static void __not_in_flash_func(cus_handle_rx_chop_start_read_sense)(pch_devib_t *devib, uint16_t count) {
        if (count > sizeof(devib->sense))
                count = sizeof(devib->sense);

        int rc = pch_dev_send_final(devib, &devib->sense, count);
        assert(rc >= 0);
        (void)rc;
}

static void __not_in_flash_func(cus_handle_rx_chop_start_read_reserved)(pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        switch (ccwcmd) {
        case PCH_CCW_CMD_SENSE:
                cus_handle_rx_chop_start_read_sense(devib, count);
                break;

        default:
                pch_dev_sense_t sense = {
                        .flags = PCH_DEV_SENSE_COMMAND_REJECT
                };
                pch_dev_update_status_error(devib, sense);
                break;
        }
}

static void __not_in_flash_func(cus_handle_rx_chop_start_read)(pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        devib->flags &= ~PCH_DEVIB_FLAG_CMD_WRITE;
        devib->size = count; // advertised window we can write to

        dmachan_start_dst_cmdbuf(&cu->rx_channel);

        if (ccwcmd >= PCH_CCW_CMD_FIRST_RESERVED)
                cus_handle_rx_chop_start_read_reserved(devib,
                        ccwcmd, count);
        else
                callback_devib(devib);
}

static void __not_in_flash_func(cus_handle_rx_chop_start_write)(pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        (void)ccwcmd; // we don't handle any reserved Write CCWs yet
        pch_cu_t *cu = pch_dev_get_cu(devib);
        devib->flags |= PCH_DEVIB_FLAG_CMD_WRITE;

        if (count == 0) {
                dmachan_start_dst_cmdbuf(&cu->rx_channel);
                callback_devib(devib);
                return;
        }

        assert(count <= devib->size);
        devib->flags |= PCH_DEVIB_FLAG_RX_DATA_REQUIRED;
        dmachan_start_dst_data(&cu->rx_channel,
                devib->addr, (uint32_t)count);
        cu->rx_active = (int16_t)pch_dev_get_ua(devib);
        // rx completion of incoming data will do Start callback
}

static void __not_in_flash_func(cus_handle_rx_chop_start)(pch_devib_t *devib, proto_packet_t p) {
        assert(!(devib->flags & PCH_DEVIB_FLAG_STARTED));
        devib->flags |= PCH_DEVIB_FLAG_STARTED;
        uint8_t ccwcmd = p.p0;
        uint16_t count = proto_decode_esize_payload(p);

	if (pch_is_ccw_cmd_write(ccwcmd))
                cus_handle_rx_chop_start_write(devib, ccwcmd, count);
        else
                cus_handle_rx_chop_start_read(devib, ccwcmd, count);
}

static void __not_in_flash_func(cus_handle_rx_command_complete)(pch_cu_t *cu) {
	// DMA has received a command packet from CSS into RxBuf
        proto_packet_t p = get_rx_packet(cu);
        pch_unit_addr_t ua = p.unit_addr;
	assert(ua < cu->num_devibs);
        pch_devib_t *devib = pch_get_devib(cu, ua);
	trace_dev_packet(PCH_TRC_RT_CUS_RX_COMMAND_COMPLETE, devib, p);
        devib->op = p.chop;
        devib->payload = proto_get_payload(p);
	switch (proto_chop_cmd(p.chop)) {
	case PROTO_CHOP_START:
		cus_handle_rx_chop_start(devib, p);
                break;

	case PROTO_CHOP_DATA:
		cus_handle_rx_chop_data(devib, p);
                break;

	case PROTO_CHOP_ROOM:
		cus_handle_rx_chop_room(devib, p);
                break;

	case PROTO_CHOP_HALT:
		cus_handle_rx_chop_halt(devib, p);
                break;

	default:
		panic("unexpected operation from CSS");
                // NOTREACHED
	}
}

static void __not_in_flash_func(cus_handle_rx_data_complete)(pch_cu_t *cu, pch_unit_addr_t ua) {
	cu->rx_active = -1;
        dmachan_start_dst_cmdbuf(&cu->rx_channel);
        pch_devib_t *devib = pch_get_devib(cu, ua);
	trace_dev(PCH_TRC_RT_CUS_RX_DATA_COMPLETE, devib);
        assert(devib->flags & PCH_DEVIB_FLAG_RX_DATA_REQUIRED);
	devib->flags &= ~PCH_DEVIB_FLAG_RX_DATA_REQUIRED;
	callback_devib(devib);
}

void __time_critical_func(pch_cus_handle_rx_complete)(pch_cu_t *cu) {
        int16_t rx_active = cu->rx_active;
	if (rx_active >= 0) {
		pch_unit_addr_t ua = (pch_unit_addr_t)rx_active;
		cus_handle_rx_data_complete(cu, ua);
	} else {
		cus_handle_rx_command_complete(cu);
	}
}
