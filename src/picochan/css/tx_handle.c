/*
 * Copyright (c) 2025 Malcolm Beattie
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

static void css_handle_tx_data_complete(css_cu_t *cu) {
	// We've just completed sending data (not a command) to the CU
	// for a device. Reread the packet to find out where we sent it.
        proto_packet_t p = get_tx_packet(cu);
	pch_unit_addr_t ua = p.unit_addr;
	pch_schib_t *schib = get_schib_by_cu(cu, ua);

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

static void css_handle_tx_command_complete(css_cu_t *cu) {
	// We've just sent a command (without any following data)
	// from TxBuf to a device on cu. Reread the packet to find out
	// where we sent it and whether we need to do anything.
        proto_packet_t p = get_tx_packet(cu);
        pch_unit_addr_t ua = p.unit_addr;
        pch_schib_t *schib = get_schib_by_cu(cu, ua);

	if (p.chop == PROTO_CHOP_START)  {
		// Start command sent with no immediate data
		css_handle_tx_start_complete(schib);
	}
}

// css_handle_tx_complete handles a tx completion interrupt for
// cu->tx_channel.
void __time_critical_func(css_handle_tx_complete)(css_cu_t *cu) {
        pch_txsm_t *txpend = &cu->tx_pending;
        PCH_CSS_TRACE_COND(PCH_TRC_RT_CSS_TX_COMPLETE,
                cu->traced, ((struct pch_trc_trdata_cu_byte){
                        .cunum = cu->cunum,
                        .byte = (uint8_t)txpend->state
                }));

        assert(cu->tx_active);
        enum pch_txsm_run_result tr = pch_txsm_run(txpend, &cu->tx_channel);
	if (tr == PCH_TXSM_ACTED)
		return; // tx dma not free - still sending pending data

	cu->tx_active = false; // tx dma is now free again

	if (tr == PCH_TXSM_FINISHED)
		css_handle_tx_data_complete(cu);
	else
		css_handle_tx_command_complete(cu);
}
