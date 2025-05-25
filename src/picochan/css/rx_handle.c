/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "ccw_fetch.h"
#include "css_trace.h"

// The returned bool is do_notify
static bool __time_critical_func(end_channel_program)(css_cu_t *cu, pch_schib_t *schib, uint8_t devs, uint16_t advcount) {
        schib->scsw.ctrl_flags &= ~PCH_AC_DEVICE_ACTIVE;
        // set the advertised window for start-write-immediate data
	schib->mda.devcount = advcount;

	// If DeviceEnd is present, then ChannelEnd should be too.
	if (!(devs & PCH_DEVS_CHANNEL_END)) {
		schib->scsw.schs |= PCH_SCHS_INTERFACE_CONTROL_CHECK;
                schib->scsw.ctrl_flags |= PCH_SC_ALERT;
		return true;
	}

	// don't try command chaining if the CfCc flag isn't set or the
	//  dev.Status is "unusual"
	uint8_t mask = PCH_DEVS_CHANNEL_END | PCH_DEVS_DEVICE_END
                | PCH_DEVS_STATUS_MODIFIER;
        bool do_chain = ((get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_CC) != 0)
                && ((devs & ~mask) == 0);
	if (!do_chain) {
                schib->scsw.ctrl_flags |= PCH_SC_SECONDARY;
		return true;
	}

	// We need to command-chain so advance the CCW address if
	// StatusModifier is set in the dev.Status
	if (devs & PCH_DEVS_STATUS_MODIFIER)
		schib->scsw.ccw_addr += sizeof(pch_ccw_t); // +8 bytes

	if (!cu->tx_active) {
		// tx engine free - send immediately
		do_command_chain_and_send_start(cu, schib);
	} else {
		// tx busy - queue up response
                pch_sid_t sid = get_sid(schib);
                push_ua_response_slist(cu, sid);
	}

	return false;
}

// do_handle_update_status handles an incoming UpdateStatus packet
// from a device or the implicit update_status after a completed rx
// of data whose Data chop had the END flag set.
// In the case that the device sends an unsolicited
// status (i.e. without ChannelEnd set), it doesn't think a channel
// program has started. Although it's probably right, it's possible
// we have just sent it a Start that crossed with its incoming
// UpdateStatus. In that situation, the device will accept (or will
// have accepted) our Start. We use FC.Start to tell whether we
// have started a channel program with it and, if so, we discard
// this unsolicited status. FC.Start can only get cleared after
// the subchannel becomes StatusPending (or via clear_subchannel)
// so Fc.Start should be an accurate way to determine this condition.
static void __time_critical_func(do_handle_update_status)(css_cu_t *cu, pch_schib_t *schib, uint8_t devs, uint16_t advcount) {
	bool do_notify = true;

	if (devs & PCH_DEVS_CHANNEL_END) {
                // ChannelEnd set: primary or primary+secondary status
                schib->scsw.ctrl_flags |= PCH_SC_PRIMARY;
                uint16_t unset = PCH_AC_SUBCHANNEL_ACTIVE
                               | PCH_FC_START;
                schib->scsw.ctrl_flags &= ~unset;
                if (devs & PCH_DEVS_DEVICE_END) {
                        // DeviceEnd: secondary status too
                        do_notify = end_channel_program(cu, schib,
                                devs, advcount);
                }
        } else {
		// ChannelEnd not set: unsolicited
                assert(!(schib->scsw.ctrl_flags & PCH_AC_DEVICE_ACTIVE));
		if (get_stashed_ccw_flags(schib) & PCH_FC_START) {
                        // discard unsolicited status for Started schib
			return;
		}
                // set advertised window for start-write-immediate data
		schib->mda.devcount = advcount;
                schib->scsw.ctrl_flags |= PCH_SC_ALERT;
	}

	if (do_notify)
		css_notify(schib, devs);
}

// handle_update_status handles an incoming UpdateStatus packet
// from a device.
static void __time_critical_func(handle_update_status)(css_cu_t *cu, pch_schib_t *schib, proto_packet_t p) {
        struct proto_parsed_devstatus_payload de
                = proto_parse_devstatus_payload(proto_get_payload(p));
        uint8_t devs = de.devs;
        uint16_t advcount = de.count;

        do_handle_update_status(cu, schib, devs, advcount);
}

typedef struct addr_count {
        uint32_t addr;
        uint16_t count;
} addr_count_t;

// begin_data_write is called from css_handle_rx_data_command to
// prepare the schib for the incoming data that's about to arrive as
// the peer device sends us data for us to receive into the current
// CCW segment of an active CCW Read-type command. As soon as we
// return with (addr, count), css_handle_rx_data_command is going to
// point the CU's rx dma engine at that destination and start it up.
// TODO If count > rescount for an incoming Data command, we could
// redirect all the about-to-be-received data to discard it, set
// ChainingCheck in Schs and then tell the device about its error
// with a Stop command. For now, we just assert.
static addr_count_t __time_critical_func(begin_data_write)(css_cu_t *cu, pch_schib_t *schib, proto_packet_t p) {
	assert(cu->rx_data_for_ua == -1);
	cu->rx_data_for_ua = (int16_t)(schib->pmcw.unit_addr);

	uint16_t count = proto_get_count(p);
	int rescount = (int)schib->scsw.count;
        assert((int)count <= rescount);

        // Propagate PROTO_CHOP_FLAG_RESPONSE_REQUIRED to the cu
        // rx_response_required flag so that, once we get the rx
        // completion of the data itself, we can see that we need to
        // do a send of a Room update
        if (proto_chop_flags(p.chop) & PROTO_CHOP_FLAG_RESPONSE_REQUIRED)
                cu->rx_response_required = true;

        // Propagate PROTO_CHOP_FLAG_END to the cu rx_data_end_ds
        // as ChannelEnd|DeviceEnd so that, once we get the rx
        // completion of the data itself, we can see that we need to
        // do an immediate update_status.
        // TODO: consider having a variant of the chop DATA command
        // that sends an esize-counted length of data and a full
        // device status in the other byte of the payload
        if (proto_chop_flags(p.chop) & PROTO_CHOP_FLAG_END) {
                uint8_t devs = PCH_DEVS_CHANNEL_END | PCH_DEVS_DEVICE_END;
                cu->rx_data_end_ds = devs;
        }

	uint32_t addr = schib->mda.data_addr;
	rescount -= (int)count;
	if (rescount == 0) {
		fetch_chain_data_ccw(schib);
		// TODO Respond with Stop or ReadError instead of assert
		assert(schib->scsw.schs == 0);
	} else {
		schib->mda.data_addr += (uint32_t)count;
		schib->scsw.count = (uint16_t)rescount;
	}

	return ((addr_count_t){addr, count});
}

static void __time_critical_func(css_handle_rx_data_complete)(css_cu_t *cu, pch_schib_t *schib) {
	cu->rx_data_for_ua = -1;
        uint8_t devs = cu->rx_data_end_ds;
	trace_schib_byte(PCH_TRC_RT_CSS_RX_DATA_COMPLETE, schib, devs);
        if (devs) {
                // implicit immediate update_status
                cu->rx_data_end_ds = 0;
                do_handle_update_status(cu, schib, devs, 0);
                return;
        }

        uint8_t mask = PCH_CCW_FLAG_PCI|PCH_CCW_FLAG_CD;
        if ((get_stashed_ccw_flags(schib) & mask) == mask) {
		// PCI flag set in ChainData CCW - notify that transfer to
		// the previous CCW segment is complete and carry on with
		// processing
		schib->scsw.ctrl_flags |= PCH_SC_INTERMEDIATE;
		css_notify(schib, 0);
	}

	if (!cu->rx_response_required)
		return;

	// Device wants a response - an UpdateRoom with how much
	// room can now be written to.
	cu->rx_response_required = false;

	if (!cu->tx_active) {
		// tx engine free - send immediately
		send_update_room(cu, schib);
	} else {
		// tx busy - queue up response
                pch_sid_t sid = get_sid(schib);
		push_ua_response_slist(cu, sid);
	}
}

// css_handle_rx_data_command handles a received _data command.
// If PROTO_CHOP_FLAG_SKIP is set then the device wants us to write
// zero bytes and will not be sending any real data itself.
// If PROTO_CHOP_FLAG_SKIP is not set then the device is going to send
// us count bytes of data and we can't stop it. It's intended to be
// written to the current CCW segment except that if the Skp CCW flag
// is set then we discard the data instead. If that happens, it means
// the device is being a bit wasteful/simplistic since it could have
// seen the Discard flag in our room announcement and used the
// PROTO_CHOP_FLAG_SKIP flag in its command instead which would have
// avoided it needing to send us all this data just for us to discard.
static void __time_critical_func(css_handle_rx_data_command)(css_cu_t *cu, pch_schib_t *schib, proto_packet_t p) {
	// if PROTO_CHOP_FLAG_SKIP is set in the incoming op, we write
	// (or ignore/discard) zeroes and no data is about to be sent
	// to us
	bool zeroes = (proto_chop_flags(p.chop) & PROTO_CHOP_FLAG_SKIP) != 0;
	// If Skp is set in the CCW then we discard the incoming data
	// (or, if PROTO_CHOP_FLAG_SKIP is set then we ignore those
	// implicit zeroes)
        bool discard = (get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_SKP) != 0;

	addr_count_t ac = begin_data_write(cu, schib, p); // may have chained
	if (discard) {
		// Skp flag set in CCW: discard data instead of writing it
		if (zeroes) {
			// The device wants us to write zeroes and
			// isn't sending data so we can bypass any
			// need to receive anything from the channel
			// and handle rx-data-complete right now
			css_handle_rx_data_complete(cu, schib);
		} else {
			// Device has gone to the trouble of sending
			// us data but so we hace to explicitly
			// receive and discard it
                        dmachan_start_dst_discard(&cu->rx_channel,
                                (uint32_t)ac.count);
		}
	} else {
		if (zeroes) {
                        dmachan_start_dst_data_src_zeroes(&cu->rx_channel,
                                ac.addr, (uint32_t)ac.count);
		} else {
                        dmachan_start_dst_data(&cu->rx_channel,
                                ac.addr, (uint32_t)ac.count);
		}
	}
}

// handle_request_read handles a RequestRead that a peer device has
// just sent us which is asking us to read count bytes of data
// from the current CCW segment (of a Write-type command) and
// send it down the channel.
static void __time_critical_func(handle_request_read)(css_cu_t *cu, pch_schib_t *schib, proto_packet_t p) {
        uint16_t count = proto_get_count(p);
        if (!(schib->scsw.ctrl_flags & PCH_SCSW_CCW_WRITE)) {
                // CU/device tried to request data when CCW is not
                // Write-type. TODO: send DataZeroes with Stop flag
                // instead of ignoring it
		schib->scsw.schs |= PCH_SCHS_INTERFACE_CONTROL_CHECK;
                schib->scsw.ctrl_flags |= PCH_SC_ALERT;
		css_notify(schib, 0);
                return;
        }

	uint16_t rescount = schib->scsw.count;
	// cap the requested count at the current segment size
	if (count > rescount)
		count = rescount;

	// stash the requested count from the device in the schib where
	// we can retrieve it if we need to defer the response because
	// the tx engine is currently busy
	schib->mda.devcount = count;

	if (!cu->tx_active) {
		// tx engine free - send immediately
		send_data_response(cu, schib);
	} else {
		// tx busy - queue up response
                pch_sid_t sid = get_sid(schib);
                push_ua_response_slist(cu, sid);
	}
}

static void __time_critical_func(css_handle_rx_command_complete)(css_cu_t *cu) {
	// DMA has received a command packet from cu into RxBuf
	proto_packet_t p = get_rx_packet(cu);
        pch_unit_addr_t ua = p.unit_addr;
        pch_schib_t *schib = get_schib_by_cu(cu, ua);
	trace_schib_packet(PCH_TRC_RT_CSS_RX_COMMAND_COMPLETE, schib, p);

	switch (proto_chop_cmd(p.chop)) {
	case PROTO_CHOP_DATA:
		css_handle_rx_data_command(cu, schib, p);
                break;
	case PROTO_CHOP_UPDATE_STATUS:
		handle_update_status(cu, schib, p);
                break;
	case PROTO_CHOP_REQUEST_READ:
		handle_request_read(cu, schib, p);
                break;
	default:
                // TODO should handle error more nicely
                panic("unexpected command from CU"); 
	}
}

void __time_critical_func(css_handle_rx_complete)(css_cu_t *cu) {
	int16_t rx_data_for_ua = cu->rx_data_for_ua;
	if (rx_data_for_ua != -1) {
		pch_unit_addr_t ua = (pch_unit_addr_t)rx_data_for_ua;
                pch_schib_t *schib = get_schib_by_cu(cu, ua);
		// Completion is for data that's just been received into
		// memory belonging to CCW address of this schib
		css_handle_rx_data_complete(cu, schib);
	} else {
		// Completion is for a command that has arrived in RxBuf.
		css_handle_rx_command_complete(cu);
	}

        rx_data_for_ua = cu->rx_data_for_ua;
	if (rx_data_for_ua == -1)
		dmachan_start_dst_cmdbuf(&cu->rx_channel);
}
