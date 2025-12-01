/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "cu_internal.h"
#include "picochan/ccw.h"
#include "cus_trace.h"

static void __not_in_flash_func(cus_handle_rx_chop_data)(pch_devib_t *devib, proto_packet_t p) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        pch_unit_addr_t ua = pch_dev_get_ua(devib);
	assert(devib->flags & PCH_DEVIB_FLAG_STARTED);
	uint32_t dstaddr = devib->addr;
	uint32_t count = (uint32_t)proto_get_count(p);
        if (proto_chop_has_skip(p.chop)) {
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
}

static void __not_in_flash_func(cus_handle_rx_chop_halt)(pch_devib_t *devib, proto_packet_t p) {
        if (!(devib->flags & PCH_DEVIB_FLAG_STARTED))
                return;

        devib->flags |= PCH_DEVIB_FLAG_STOPPING;
}

static void __not_in_flash_func(cus_handle_rx_chop_start_read)(pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        devib->flags &= ~PCH_DEVIB_FLAG_CMD_WRITE;
        devib->size = count; // advertised window we can write to

        dmachan_start_dst_cmdbuf(&cu->rx_channel);
}

static void __not_in_flash_func(cus_handle_rx_chop_start_write)(pch_devib_t *devib, uint8_t ccwcmd, uint16_t count) {
        (void)ccwcmd; // we don't handle any reserved Write CCWs yet
        pch_cu_t *cu = pch_dev_get_cu(devib);
        devib->flags |= PCH_DEVIB_FLAG_CMD_WRITE;

        if (count == 0) {
                dmachan_start_dst_cmdbuf(&cu->rx_channel);
                return;
        }

        assert(count <= devib->size);
        assert(cu->rx_active == -1);
        cu->rx_active = (int16_t)pch_dev_get_ua(devib);
        dmachan_start_dst_data(&cu->rx_channel,
                devib->addr, (uint32_t)count);
        // rx completion of incoming data will do callback
}

static void __not_in_flash_func(cus_handle_rx_chop_start)(pch_devib_t *devib, proto_packet_t p) {
        assert(!pch_devib_is_started(devib));
        if (pch_devib_is_started(devib)) {
                pch_dev_update_status_proto_error(devib);
                return;
        }

        devib->flags |= PCH_DEVIB_FLAG_START_PENDING;
        uint8_t ccwcmd = p.p0;
        uint16_t count = proto_decode_esize_payload(p);

	if (pch_is_ccw_cmd_write(ccwcmd))
                cus_handle_rx_chop_start_write(devib, ccwcmd, count);
        else
                cus_handle_rx_chop_start_read(devib, ccwcmd, count);
}

static inline proto_packet_t get_rx_packet(dmachan_link_t *l) {
        return *(proto_packet_t *)&l->cmd;
}

static pch_devib_t *__not_in_flash_func(cus_handle_rx_command_complete)(pch_cu_t *cu) {
	// DMA has received a command packet from CSS into RxBuf
        dmachan_link_t *rxl = &cu->rx_channel.link;
        proto_packet_t p = get_rx_packet(rxl);
        pch_unit_addr_t ua = p.unit_addr;
	assert(ua < cu->num_devibs);
        pch_devib_t *devib = pch_get_devib(cu, ua);
	trace_dev_packet(PCH_TRC_RT_CUS_RX_COMMAND_COMPLETE, devib, p,
                dmachan_link_seqnum(rxl));
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

        return devib;
}

static void __not_in_flash_func(cus_handle_rx_data_complete)(pch_cu_t *cu, pch_devib_t *devib) {
	cu->rx_active = -1;
        dmachan_start_dst_cmdbuf(&cu->rx_channel);
	trace_dev(PCH_TRC_RT_CUS_RX_DATA_COMPLETE, devib);
}

void __time_critical_func(pch_cus_handle_rx_complete)(pch_cu_t *cu) {
        int16_t rx_active = cu->rx_active;
        pch_devib_t *devib;
	if (rx_active >= 0) {
                devib = pch_get_devib(cu, (pch_unit_addr_t)rx_active);
		cus_handle_rx_data_complete(cu, devib);
	} else {
		devib = cus_handle_rx_command_complete(cu);
	}

        if (cu->rx_active >= 0)
                return; // receiving data following Data or Start

        if (pch_devib_is_tx_busy(devib)) {
                // defer callback until tx completion
                pch_devib_set_callback_pending(devib, true);
        } else {
                pch_devib_schedule_callback(devib);
        }
}
