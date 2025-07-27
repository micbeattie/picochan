/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "ccw_fetch.h"
#include "css_trace.h"

// send_command_with_data is invoked by send_data_response (using
// PROTO_CHOP_DATA) and send_start_packet (using PROTO_CHOP_START
// when immediate data is to be sent). It consumes and sends count
// bytes of data from the current segment (when the CCW Skp flag is
// not set) or generates count bytes of implicit zeroes as though
// from the segment (if Skp is set).  It builds and sends a command
// packet using p, ORring in flags Skip, End and Stop to the Chop
// field as needed. If the Skip op flag is not set then it also
// arranges for the TxPending state machine to transmit the actual
// data immediately after the command itself is transmitted.
void __time_critical_func(send_command_with_data)(css_cu_t *cu, pch_schib_t *schib, proto_packet_t p, uint16_t count) {
        assert(!cu->tx_active);

	uint32_t addr = 0; // not used if zeroes is set

        bool zeroes = (get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_SKP) != 0;
	if (zeroes)
		p.chop |= PROTO_CHOP_FLAG_SKIP;
        else
		addr = schib->mda.data_addr;

        assert(count != 0);
	uint16_t rescount = schib->scsw.count;
        assert(count <= rescount);

	rescount -= count;
	if (rescount > 0) {
		if (!zeroes)
			schib->mda.data_addr += (uint32_t)count;

		schib->scsw.count = rescount;
	} else {
		// segment finished - try data chaining for the next
                fetch_chain_data_ccw(schib);
		if (schib->scsw.schs != 0)
			p.chop |= PROTO_CHOP_FLAG_STOP;

		if (schib->scsw.count == 0)
			p.chop |= PROTO_CHOP_FLAG_END;
	}

	if (!zeroes)
                pch_txsm_set_pending(&cu->tx_pending, addr, count);

        trace_schib_packet(PCH_TRC_RT_CSS_SEND_TX_PACKET, schib, p);
	send_tx_packet(cu, p);
}

void __time_critical_func(send_data_response)(css_cu_t *cu, pch_schib_t *schib) {
        proto_chop_flags_t chopfl = 0;
        uint16_t count = schib->mda.devcount;

        uint16_t rescount = schib->scsw.count;
        // if the requested count exceeds the current segment size
        // then cap the resulting data length but also the CCW SLI,
        // CD and CC flags affect what we do
        if (count > rescount) {
                count = rescount;

                pch_ccw_flags_t ccwfl = get_stashed_ccw_flags(schib);
                if (!(ccwfl & PCH_CCW_FLAG_CD)) {
                        chopfl = PROTO_CHOP_FLAG_STOP;
                        if (!(ccwfl & PCH_CCW_FLAG_SLI))
                                schib->scsw.schs |= PCH_SCHS_INCORRECT_LENGTH;
                }
        }

        proto_chop_t chop = PROTO_CHOP_DATA | chopfl;
        pch_unit_addr_t ua = schib->pmcw.unit_addr;
        proto_packet_t p = proto_make_count_packet(chop, ua, count);
	send_command_with_data(cu, schib, p, count);
}

void __time_critical_func(send_update_room)(css_cu_t *cu, pch_schib_t *schib) {
        assert(!cu->tx_active);

	proto_chop_t op = PROTO_CHOP_ROOM;
        if (schib->scsw.schs != 0)
		op |= PROTO_CHOP_FLAG_STOP;

        pch_unit_addr_t ua = schib->pmcw.unit_addr;
	proto_packet_t p = proto_make_count_packet(op, ua,
                schib->scsw.count);
        trace_schib_packet(PCH_TRC_RT_CSS_SEND_TX_PACKET, schib, p);
        send_tx_packet(cu, p);
}

void __time_critical_func(do_command_chain_and_send_start)(css_cu_t *cu, pch_schib_t *schib) {
        assert(!cu->tx_active);

        uint8_t ccwcmd = fetch_chain_command_ccw(schib);
        if (schib->scsw.schs != 0) {
                schib->scsw.ctrl_flags &= ~(PCH_AC_SUBCHANNEL_ACTIVE|PCH_AC_DEVICE_ACTIVE);
		schib->scsw.ctrl_flags |= PCH_SC_ALERT;
		css_notify(schib, 0);
		return;
	}

        
        if (get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_PCI) {
		// PCI flag set - notify that channel program has got to
		// here and carry on with processing
                schib->scsw.ctrl_flags |= PCH_SC_INTERMEDIATE;
		css_notify(schib, 0);
	}

	suspend_or_send_start_packet(cu, schib, ccwcmd);
}

void __time_critical_func(process_schib_response)(css_cu_t *cu, pch_schib_t *schib) {
	assert(!cu->tx_active);
        uint16_t ctrl_flags = schib->scsw.ctrl_flags;
        if (!(ctrl_flags & PCH_AC_DEVICE_ACTIVE)) {
		// no active device means the device must have sent
		// an UpdateStatus with DeviceEnd, and the response
		// we need to generate is a command-chain followed
		// by sending a Start command with that new CCW.
		do_command_chain_and_send_start(cu, schib);
        } else if (ctrl_flags & PCH_SCSW_CCW_WRITE) {
		// CCW is Write-type so the response we need to
		// generate must be to an incoming RequestRead.
		// That means we need to send a Data+data for the
		// requested size of data (which has been stashed
		// in schib.mda.devcount) and do a chain-data if
		// our sent data is going to empty the segment and
		// the CCW has the ChainData flag present.
		send_data_response(cu, schib);
	} else {
		// CCW is Read-type so the response we need to
		// generate must be to an incoming Data+data. That
		// means we need to send an UpdateRoom with the
		// size of the new segment (or zero if there was no
		// chain-data or the chain-data failed).
		send_update_room(cu, schib);
	}
}
