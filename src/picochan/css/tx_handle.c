/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#include "css_internal.h"
#include "css_trace.h"
#include "txsm/txsm.h"
#include "proto/packet.h"

// css_handle_tx_start_complete handles the completion of sending
// either a Start command with no immediate data or the combination of
// a (Write-type) Start command followed immediately by some immediate
// data.
static void css_handle_tx_start_complete(pch_schib_t *schib) {
	schib->scsw.ctrl_flags |= (PCH_AC_SUBCHANNEL_ACTIVE
                | PCH_AC_DEVICE_ACTIVE);

        if (get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_PCI) {
		// PCI flag set - notify that channel program has started
		// and carry on with processing
		schib->scsw.ctrl_flags |= PCH_SC_INTERMEDIATE;
		css_notify(schib, 0);
	}
}

// css_handle_tx_data_after_data_complete handles the completion of
// sending data following a Data command.
static void css_handle_tx_data_after_data_complete(pch_schib_t *schib) {
        uint8_t mask = PCH_CCW_FLAG_PCI|PCH_CCW_FLAG_CD;
	if ((get_stashed_ccw_flags(schib) & mask) == mask) {
		// PCI flag set in ChainData CCW - notify that transfer from
		// the previous CCW segment is complete and carry on with
		// processing
                schib->scsw.ctrl_flags |= PCH_SC_INTERMEDIATE;
		css_notify(schib, 0);
	}
}

static void css_handle_tx_data_complete(pch_chp_t *chp) {
	// We've just completed sending data (not a command) to the CU
	// for a device. Reread the packet to find out where we sent it.
        proto_packet_t p = get_tx_packet(chp);
	pch_unit_addr_t ua = p.unit_addr;
	pch_schib_t *schib = get_schib_by_chp(chp, ua);

	// TODO Check DMA registers for errors and try to deal with any

	switch (proto_chop_cmd(p.chop)) {
	case PROTO_CHOP_START:
		// Start command sent with immediate data
		css_handle_tx_start_complete(schib);
                break;

	case PROTO_CHOP_DATA:
		css_handle_tx_data_after_data_complete(schib);
                break;

	default:
                // TODO should handle error more nicely
                panic("unexpected tx packet");
	}
}

static void css_handle_tx_command_complete(pch_chp_t *chp) {
	// We've just sent a command (without any following data)
	// from TxBuf to a device on chp. Reread the packet to find out
	// where we sent it and whether we need to do anything.
        proto_packet_t p = get_tx_packet(chp);
        pch_unit_addr_t ua = p.unit_addr;
        pch_schib_t *schib = get_schib_by_chp(chp, ua);

	if (p.chop == PROTO_CHOP_START)  {
		// Start command sent with no immediate data
		css_handle_tx_start_complete(schib);
	}
}

// css_handle_tx_complete handles a tx completion for
// chp->tx_channel. It is called either from the DMA IRQ handler
// after a DMA tx completes or directly from send_tx_packet() if
// the packet was sent synchronously via memory channel as indicated
// by the dmachan link's txl->complete flag being set.
void __time_critical_func(css_handle_tx_complete)(pch_chp_t *chp) {
        pch_txsm_t *txpend = &chp->tx_pending;
        PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_TX_COMPLETE,
                pch_chp_is_traced_irq(chp), ((struct pch_trdata_id_byte){
                        .id = pch_get_chpid(chp),
                        .byte = (uint8_t)txpend->state
                }));

        assert(pch_chp_is_tx_active(chp));
        enum pch_txsm_run_result tr = pch_txsm_run(txpend, &chp->tx_channel);
	if (tr == PCH_TXSM_ACTED)
		return; // tx dma not free - still sending pending data

	pch_chp_set_tx_active(chp, false); // tx dma is now free again

	if (tr == PCH_TXSM_FINISHED)
		css_handle_tx_data_complete(chp);
	else
		css_handle_tx_command_complete(chp);
}
