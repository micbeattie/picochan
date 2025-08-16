/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include <stdint.h>
#include <assert.h>
#include "schib_internal.h"
#include "picochan/css.h"
#include "css_trace.h"

// fetch_ccw fetches an 8-byte CCW from bus address addr, which must
// be 4-byte aligned. When marshalling/unmarshalling a CCW, unlike the
// original architected Format-1 CCW which was implicitly big-endian,
// the count and addr fields here are treated as native-endian and so
// will be little-endian on both ARM and RISC-V (in Pico
// configurations) and would also be so on x86, for example.
static inline pch_ccw_t fetch_ccw(pch_ccw_t *addr) {
        valid_params_if(PCH_CSS,
                ((uint32_t)addr & 0x3) == 0); // require 4-byte alignment
        return *addr;
}

static inline void update_ccw_cmd_write_flag(pch_schib_t *schib, uint8_t ccwcmd) {
        if (pch_is_ccw_cmd_write(ccwcmd))
                schib->scsw.ctrl_flags |= PCH_SCSW_CCW_WRITE;
        else
                schib->scsw.ctrl_flags &= ~PCH_SCSW_CCW_WRITE;
}

// update_ccw_fields updates schib fields with all non-command fields
// of CCW and ccw_addr.
static inline void update_ccw_fields(pch_schib_t *schib, pch_ccw_t *ccw_addr, pch_ccw_t ccw) {
	schib->scsw.ccw_addr = (uint32_t)ccw_addr;
	schib->scsw.devs = (uint8_t)ccw.flags;
	schib->scsw.count = ccw.count;
	schib->mda.data_addr = ccw.addr;
}

// fetch_first_command_ccw uses fetch_ccw to fetch the CCW pointed to
// by schib->scsw.ccw_ddr, validates it as a first CCW of a channel
// program, then stores all fields except ccw.cmd into the schib, sets
// the PCH_SCSW_CCW_WRITE flag in schib->scsw.ctrl_flags to 1 or 0
// based on whether ccw.cmd is a Write-type command or not and returns
// that ccw.cmd. If there is an error, an appropriate flag is set in
// schib->scsw.schs.
uint8_t __time_critical_func(fetch_first_command_ccw)(pch_schib_t *schib) {
	pch_ccw_t *ccw_addr = (pch_ccw_t*)schib->scsw.ccw_addr;
	pch_ccw_t ccw = fetch_ccw(ccw_addr);
	trace_schib_ccw(PCH_TRC_RT_CSS_CCW_FETCH, schib, ccw_addr, ccw);
	ccw_addr++;

	// TODO include other validity checks
	if (ccw.cmd == PCH_CCW_CMD_TIC) {
		schib->scsw.schs |= PCH_SCHS_PROGRAM_CHECK;
		return 0;
	}

	update_ccw_fields(schib, ccw_addr, ccw);
	update_ccw_cmd_write_flag(schib, ccw.cmd);

	return ccw.cmd;
}

// fetch_resume_ccw uses fetch_ccw to fetch the CCW pointed to by
// schib->scsw.ccw_addr - 1, i.e. 8 bytes before, thus the same CCW
// address that was previously fetched before this (assumed) Resume,
// validates it as a first CCW of a channel program, then stores all
// fields except ccw.cmd into the schib, sets the PCH_SCSW_CCW_WRITE
// flag in schib->scsw.ctrl_flags to 1 or 0 based on whether ccw.cmd
// is a Write-type command or not and returns that ccw.cmd. If there
// is an error, an appropriate flag is set in schib->scsw.schs.
uint8_t __time_critical_func(fetch_resume_ccw)(pch_schib_t *schib) {
	// fetch CCW from preceding location
	pch_ccw_t *ccw_addr = (pch_ccw_t*)schib->scsw.ccw_addr;
        ccw_addr--; // -8 bytes
	pch_ccw_t ccw = fetch_ccw(ccw_addr);
	trace_schib_ccw(PCH_TRC_RT_CSS_CCW_FETCH, schib, ccw_addr, ccw);
	// Don't increment schib->scsw.ccw_addr

	// TODO include other validity checks
	if (ccw.cmd == PCH_CCW_CMD_TIC) {
		schib->scsw.schs |= PCH_SCHS_PROGRAM_CHECK;
		return 0;
	}

	update_ccw_fields(schib, ccw_addr, ccw);
	update_ccw_cmd_write_flag(schib, ccw.cmd);

	return ccw.cmd;
}

// fetch_chain_ccw uses fetch_ccw to fetch the CCW pointed to by
// schib->scsw.ccw_addr, follows valid TICs then stores all fields
// except ccw.cmd into the schib and returns that ccw.cmd. If there
// is an error, an appropriate flag is set in schib->scsw.schs.
uint8_t __time_critical_func(fetch_chain_ccw)(pch_schib_t *schib) {
        pch_ccw_t *ccw_addr = (pch_ccw_t*)schib->scsw.ccw_addr;
	pch_ccw_t ccw = fetch_ccw(ccw_addr);
	trace_schib_ccw(PCH_TRC_RT_CSS_CCW_FETCH, schib, ccw_addr, ccw);
        ccw_addr++; // +8 bytes

	if (ccw.cmd == PCH_CCW_CMD_TIC) {
                ccw_addr = pch_ccw_get_addr(ccw);
		ccw = fetch_ccw(ccw_addr);
                trace_schib_ccw(PCH_TRC_RT_CSS_CCW_FETCH, schib,
                        ccw_addr, ccw);
		ccw_addr++; // +8 bytes
                if (ccw.cmd == PCH_CCW_CMD_TIC) {
			schib->scsw.schs |= PCH_SCHS_PROGRAM_CHECK;
			return 0;
		}
	}

        update_ccw_fields(schib, ccw_addr, ccw);

	return ccw.cmd;
}

// fetch_chain_data_ccw fetches and validates the next CCW in a CCW
// data-chain, if needed. If the chain-data flag is set in the
// schib's current CCW flags, then fetch_chain_ccw is used to fetch
// CCWs following TICs and the resulting CCW is validated. If there
// is an error while fetching or the fetched CCW is invalid then
// schib->scsw.count is set to zero and an appropriate error flag
// is set in schib->scsw.schs.
void __time_critical_func(fetch_chain_data_ccw)(pch_schib_t *schib) {
        if (!(get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_CD)) {
		// ChainData not set so nothing to do - not an error
		schib->scsw.count = 0;
		return;
	}

        (void)fetch_chain_ccw(schib); // ignore returned ccwcmd
	if (schib->scsw.schs != 0) {
                // Fetch error
		schib->scsw.count = 0;
		return;
	}

        if ((get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_S)) {
		// Suspend flag not allowed when data chaining
		schib->scsw.count = 0;
                schib->scsw.schs |= PCH_SCHS_PROGRAM_CHECK;
		return;
	}

        // successful chain data
}

// fetch_chain_command_ccw fetches and validates the next CCW in a CCW
// command-chain. The chain-command flag must already be set in the
// schib's current CCW flags or else it panics. fetch_chain_ccw is
// used to fetch CCWs following TICs, the resulting CCW is validated
// and its ccw.cmd is returned. If there is an error while fetching or
// the fetched CCW is invalid then an appropriate error flag is set
// in schib->scsw.schs.
uint8_t __time_critical_func(fetch_chain_command_ccw)(pch_schib_t *schib) {
        valid_params_if(PCH_CSS,
                get_stashed_ccw_flags(schib) & PCH_CCW_FLAG_CC);
        uint8_t ccwcmd = fetch_chain_ccw(schib);
	if (schib->scsw.schs != 0)
		return 0;

	// TODO maybe need more validity checks
	update_ccw_cmd_write_flag(schib, ccwcmd);

	return ccwcmd;
}
