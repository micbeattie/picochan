/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "ccw_fetch.h"
#include "css_trace.h"

static void suspend(pch_schib_t *schib) {
        schib->scsw.ctrl_flags &= ~(PCH_AC_SUBCHANNEL_ACTIVE|PCH_AC_DEVICE_ACTIVE);
        schib->scsw.ctrl_flags |= (PCH_AC_SUSPENDED|PCH_SC_INTERMEDIATE);
	css_notify(schib, 0);
}

// send_start_packet builds and sends a Start packet to the CU.
// If the CCW is a Write-type command, there is data in the
// current CCW segment and the device has previously advertised
// a non-zero window for us to write into then data from the
// segment is schedule to follow the start packet in the same way
// as a Data command is sent. The amount of data sent is
// limited to the minimum of the device-advertised window size,
// the segment size and the bsize-encoding of those.
// For a Read-type CCW, the count we encode into the payload is
// the current CCW segment size which advertises how much data
// the device can send us with Data+data.
static void send_start_packet(pch_chp_t *chp, pch_schib_t *schib, uint8_t ccwcmd) {
        uint16_t count = schib->scsw.count;

        bool write = (schib->scsw.ctrl_flags & PCH_SCSW_CCW_WRITE) != 0;
	if (write) {
                uint16_t advcount = schib->mda.devcount;
		if (count > advcount)
			count = advcount;
	}

	pch_unit_addr_t ua = schib->pmcw.unit_addr;
        pch_bsize_t esize = pch_bsize_encode(count);
        proto_packet_t p = proto_make_esize_packet(PROTO_CHOP_START,
                ua, ccwcmd, esize);
	if (write && count > 0) {
		count = pch_bsize_decode(esize);
		send_command_with_data(chp, schib, p, count);
	} else {
                trace_schib_packet(PCH_TRC_RT_CSS_SEND_TX_PACKET,
                        schib, p);
		send_tx_packet(chp, p);
	}
}

void __time_critical_func(suspend_or_send_start_packet)(pch_chp_t *chp, pch_schib_t *schib, uint8_t ccwcmd) {
        assert(!chp->tx_active);

        if (get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_S)
		suspend(schib); // CCW Suspend flag set
	else
		send_start_packet(chp, schib, ccwcmd);
}

static void process_schib_start(pch_schib_t *schib) {
        schib->scsw.ctrl_flags &= ~PCH_SC_MASK;
        schib->scsw.ctrl_flags &= ~PCH_AC_START_PENDING;
        schib->scsw.ctrl_flags |= PCH_FC_START;

	pch_chpid_t chpid = schib->pmcw.chpid;
        pch_chp_t *chp = pch_get_chp(chpid);
        uint8_t ccwcmd = fetch_first_command_ccw(schib);
        if (schib->scsw.schs != 0) {
                // XXX something like the following but this is probably
                // not quite right. We set CC=1 (a 2-bit value)
                schib->scsw.user_flags &= ~PCH_SF_CC_MASK;
                schib->scsw.user_flags |= (1 << PCH_SF_CC_SHIFT);
                schib->scsw.ctrl_flags |= PCH_SC_ALERT;
                css_notify(schib, 0);
                return;
        }

	suspend_or_send_start_packet(chp, schib, ccwcmd);
}

static void process_schib_resume(pch_schib_t *schib) {
        schib->scsw.ctrl_flags &= ~PCH_SC_MASK;
        schib->scsw.ctrl_flags &= ~PCH_AC_RESUME_PENDING;
        schib->scsw.ctrl_flags |= PCH_FC_START; // XXX set this or not?

	pch_chpid_t chpid = schib->pmcw.chpid;
        pch_chp_t *chp = pch_get_chp(chpid);
        uint8_t ccwcmd = fetch_resume_ccw(schib);
        if (schib->scsw.schs != 0) {
                schib->scsw.ctrl_flags |= PCH_SC_ALERT;
                css_notify(schib, 0);
                return;
        }

	suspend_or_send_start_packet(chp, schib, ccwcmd);
}

// process_schib_func processes a schib which has been put on
// the pending list for processing by preparing and sending a channel
// operation to a CU. For now, that's mainly for a Start but at some
// point we'll probably need to implement Resume, Halt and Clear too
// (and maybe Stop for some errors will come via this path too).
void __time_critical_func(process_schib_func)(pch_schib_t *schib) {
        schib->scsw.schs = 0;
        uint16_t ctrl_flags = schib->scsw.ctrl_flags;
        if (ctrl_flags & PCH_AC_START_PENDING) {
		process_schib_start(schib);
		return;
	}

        if (ctrl_flags & PCH_AC_RESUME_PENDING) {
		process_schib_resume(schib);
		return;
	}

        // Halt and Clear not yet implemented
        assert(!(ctrl_flags & PCH_AC_HALT_PENDING));
        assert(!(ctrl_flags & PCH_AC_CLEAR_PENDING));

        // no activity pending - no-op
}

