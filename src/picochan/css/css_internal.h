/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CSS_CSS_INTERNAL_H
#define _PCH_CSS_CSS_INTERNAL_H

// PICO_CONFIG: PARAM_ASSERTIONS_ENABLED_PCH_CSS, Enable/disable assertions in the pch_css module, type=bool, default=0, group=pch_css
#ifndef PARAM_ASSERTIONS_ENABLED_PCH_CSS
#define PARAM_ASSERTIONS_ENABLED_PCH_CSS 0
#endif

#include <stdint.h>
#include <assert.h>
#include "hardware/sync.h"
#include "hardware/irq.h"
#include "picochan/css.h"
#include "schibs_lock.h"
#include "schib_internal.h"
#include "schib_dlist.h"
#include "channel.h"
#include "picochan/dmachan.h"
#include "trc/trace.h"

/*! \file css/css_internal.h
 *  \defgroup internal_css internal_css
 *
 * \brief internal CSS implementations
 */

/*! \brief struct css is a channel subsystem (CSS)
 *
 * It is intended to be a singleton and is just a convenience for
 * gathering together the global variables associated with the CSS.
 */
struct css {
        schib_dlist_t   isc_dlists[PCH_NUM_ISCS]; // indexed by ISC
        io_callback_t   io_callback;
        int16_t         io_irqnum;   //!< -1 or Irq raised for schib notify
        int16_t         func_irqnum; //!< raised by API to schedule schib function
        uint8_t         isc_enable_mask;
        uint8_t         isc_status_mask;
        int8_t          dmairqix; //!< completions raise irq dma.IRQ_BASE+dmairqix
        int8_t          core_num;
        pch_sid_t       next_sid; //!< starting SID for next pch_chp_claim
        pch_trc_bufferset_t trace_bs;
        pch_chp_t       chps[PCH_NUM_CHANNELS];
        pch_schib_t     schibs[PCH_NUM_SCHIBS];
};

extern struct css CSS;

static inline pch_schib_t *get_schib(pch_sid_t sid) {
        return &CSS.schibs[sid];
}

static inline pch_chp_t *pch_get_chp(pch_chpid_t chpid) {
        return &CSS.chps[chpid];
}

static inline pch_chpid_t pch_get_chpid(pch_chp_t *chp) {
        int32_t n = chp - &CSS.chps[0];
        assert(n >= 0 && n < PCH_NUM_CHANNELS);
        return (pch_chpid_t)n;
}

static inline schib_dlist_t *get_isc_dlist(uint8_t iscnum) {
        valid_params_if(PCH_CSS, iscnum < PCH_NUM_ISCS);
        return &CSS.isc_dlists[iscnum];
}

static inline pch_schib_t *get_schib_by_chp(pch_chp_t *chp, pch_unit_addr_t ua) {
        valid_params_if(PCH_CSS, (uint16_t)ua < chp->num_devices);
        return get_schib(chp->first_sid + (pch_sid_t)ua);
}

static inline pch_sid_t get_sid(pch_schib_t *schib) {
        // if we definitely decide to include intparm in the PMCW then
        // the schib size is 32 bytes so we could easily check the low
        // 5 bits are all zero as a valid_params_if check too.
        valid_params_if(PCH_CSS,
                schib >= &CSS.schibs[0]
                && schib < &CSS.schibs[PCH_NUM_SCHIBS]);

        return schib - CSS.schibs;
}

static inline bool css_is_started(void) {
        return CSS.dmairqix >= 0;
}

static inline uint8_t css_get_configured_dmairqix(void) {
        assert(css_is_started());
        return (uint8_t)CSS.dmairqix;
}

static inline void reset_subchannel_to_idle(pch_schib_t *schib) {
        const uint16_t mask = PCH_FC_START|PCH_FC_HALT|PCH_FC_CLEAR
                | PCH_AC_RESUME_PENDING|PCH_AC_START_PENDING
                | PCH_AC_HALT_PENDING|PCH_AC_CLEAR_PENDING
                | PCH_AC_SUSPENDED | PCH_SC_PENDING;

        schib->scsw.ctrl_flags &= ~mask;
}

static inline void css_clear_pending_subchannel(pch_schib_t *schib) {
        valid_params_if(PCH_CSS, schib_is_status_pending(schib));

        if (schib->scsw.ctrl_flags & PCH_SC_INTERMEDIATE) {
                // TODO Don't do clearing unless various flag
                // combinations are set.
        }

        reset_subchannel_to_idle(schib);
}

void __isr pch_css_dma_irq_handler(void);

void suspend_or_send_start_packet(pch_chp_t *chp, pch_schib_t *schib, uint8_t ccwcmd);
void do_command_chain_and_send_start(pch_chp_t *chp, pch_schib_t *schib);
void send_command_with_data(pch_chp_t *chp, pch_schib_t *schib, proto_packet_t p, uint16_t count);
void send_update_room(pch_chp_t *chp, pch_schib_t *schib);
void send_data_response(pch_chp_t *chp, pch_schib_t *schib);
void css_handle_rx_complete(pch_chp_t *chp);
void css_handle_tx_complete(pch_chp_t *chp);

//
// isc dlists
//

pch_schib_t *pop_pending_schib_from_isc(uint8_t iscnum);
void remove_from_isc_dlist(uint8_t iscnum, pch_sid_t sid);
void push_to_isc_dlist(pch_schib_t *schib);
pch_schib_t *pop_pending_schib(void);
void css_notify(pch_schib_t *schib, uint8_t devs);

static inline pch_intcode_t css_make_intcode(pch_schib_t *schib) {
        pch_intcode_t ic = { 0 }; // all fields zeroes, including cc
        if (schib) {
                pch_sid_t sid = get_sid(schib);
                ic.intparm = schib->pmcw.intparm;
                ic.sid = sid;
                ic.flags = pch_pmcw_isc(&schib->pmcw);
                ic.cc = 1; // cc=1 means intcode stored [sic]
        }

        return ic;
}

#endif
