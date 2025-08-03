/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "css_trace.h"

static inline void raise_func_irq(void) {
        int16_t n = CSS.func_irqnum;
        valid_params_if(PCH_CSS, n > 0);
        irq_set_pending((irq_num_t)n);
}

// push_func_dlist must be called with schibs_lock held.
static inline void push_func_dlist(pch_chp_t *chp, pch_schib_t *schib) {
        push_ua_dlist_unsafe(&chp->ua_func_dlist, chp, schib);
}

static int schib_is_ready_for_start_or_resume(pch_schib_t *schib) {
	if (!schib_is_enabled(schib))
                return 3; // cc3 means schib not enabled

        if (schib_has_function_in_progress(schib))
                return 2; // cc2 means a function is already in progress

        if (schib_is_status_pending(schib))
                return 1; // cc1 means status pending

        return 0;
}

static int do_sch_start(pch_schib_t *schib, pch_ccw_t *ccw_addr) {
        uint32_t status = schibs_lock();

        int cc = schib_is_ready_for_start_or_resume(schib);
        if (cc != 0)
                goto out;

        assert(schib->mda.nextsid == get_sid(schib)); // shouldn't be on a list
	pch_chpid_t chpid = schib->pmcw.chpid;
	pch_chp_t *chp = pch_get_chp(chpid);
	schib->scsw.ccw_addr = (uint32_t)ccw_addr;
	schib->scsw.ctrl_flags |= PCH_AC_START_PENDING;
        push_func_dlist(chp, schib);
	raise_func_irq();

out:
        schibs_unlock(status);
	return cc;
}

int __time_critical_func(pch_sch_start)(pch_sid_t sid, pch_ccw_t *ccw_addr) {
	if (sid >= PCH_NUM_SCHIBS)
		return 3;

	pch_schib_t *schib = get_schib(sid);
	int cc = do_sch_start(schib, ccw_addr);
	trace_schib_word_byte(PCH_TRC_RT_CSS_SCH_START, schib,
                (uint32_t)ccw_addr, cc);
	return cc;
}

static int do_sch_resume(pch_schib_t *schib) {
        uint32_t status = schibs_lock();

        int cc = schib_is_ready_for_start_or_resume(schib);
        if (cc != 0)
                goto out;

        assert(schib->mda.nextsid == get_sid(schib)); // shouldn't be on a list

        pch_chpid_t chpid = schib->pmcw.chpid;
        pch_chp_t *chp = pch_get_chp(chpid);
	schib->scsw.ctrl_flags |= PCH_AC_RESUME_PENDING;
        push_func_dlist(chp, schib);
        raise_func_irq();

out:
        schibs_unlock(status);
	return cc;
}

int __time_critical_func(pch_sch_resume)(pch_sid_t sid) {
        pch_schib_t *schib = get_schib(sid);
	int cc = do_sch_resume(schib);
        trace_schib_byte(PCH_TRC_RT_CSS_SCH_RESUME, schib, cc);
	return cc;
}

static int schib_is_valid_for_cancel(pch_schib_t *schib) {
	if (!schib_is_enabled(schib))
                return 3; // cc3 means schib not enabled

        if (schib_is_status_pending(schib))
                return 1; // cc1 means status pending

        uint16_t ctrl_flags = schib->scsw.ctrl_flags;
        if ((ctrl_flags & PCH_FC_MASK) != PCH_FC_START)
                return 2; // cc2 for function other than just start
        
        if (ctrl_flags & PCH_AC_SUBCHANNEL_ACTIVE)
                return 2; // cc2 for subchannel being active

        const uint16_t mask = PCH_AC_RESUME_PENDING
                | PCH_AC_START_PENDING
                | PCH_AC_SUSPENDED;
        // cc2 unless start pending, resume pending or suspended
        if (!(ctrl_flags & mask))
                return 2;

        return 0;
}

// remove_from_func_dlist must be called with schibs_lock held.
static void remove_from_func_dlist(pch_schib_t *schib) {
        pch_chpid_t chpid = schib->pmcw.chpid;
        pch_chp_t *chp = pch_get_chp(chpid);
        ua_dlist_t *l = &chp->ua_func_dlist;
        pch_unit_addr_t ua = schib->pmcw.unit_addr;
        remove_from_ua_dlist_unsafe(l, chp, ua);
}

static void remove_from_notify_list(pch_schib_t *schib) {
        pch_sid_t sid = get_sid(schib);
        remove_from_isc_dlist(pch_pmcw_isc(&schib->pmcw), sid);
}

static int do_sch_cancel(pch_schib_t *schib) {
        uint32_t status = schibs_lock();

        int cc = schib_is_valid_for_cancel(schib);
        if (cc != 0)
                goto out;

        uint16_t ctrl_flags = schib->scsw.ctrl_flags;
        // remove schib from whichever list it is on: if schib is
        // suspended, it is on the notify list, otherwise it is
        // either start pending or resume pending in which case it
        // is on the function list of its channel
        if (ctrl_flags & PCH_AC_SUSPENDED)
                remove_from_notify_list(schib);
        else
                remove_from_func_dlist(schib);

        reset_subchannel_to_idle(schib);

out:
        schibs_unlock(status);
	return cc;
}

int __time_critical_func(pch_sch_cancel)(pch_sid_t sid) {
	if (sid >= PCH_NUM_SCHIBS)
		return 3;

	pch_schib_t *schib = get_schib(sid);
	int cc = do_sch_cancel(schib);
	trace_schib_byte(PCH_TRC_RT_CSS_SCH_CANCEL, schib, cc);
	return cc;
}

// caller must ensure *loc_scsw is in fast RAM
static int do_sch_test(pch_schib_t *schib, pch_scsw_t *loc_scsw) {
        int cc = 1;
        uint32_t status = schibs_lock();

	*loc_scsw = schib->scsw;
	if (!schib_is_status_pending(schib))
		goto out; // with cc1

        remove_from_notify_list(schib);
	css_clear_pending_subchannel(schib);
        cc = 0;

out:
        schibs_unlock(status);
	return cc;
}

int __time_critical_func(pch_sch_test)(pch_sid_t sid, pch_scsw_t *scsw) {
	pch_scsw_t loc_scsw; // must be on stack (fast RAM)
	if (sid >= PCH_NUM_SCHIBS)
		return 3;

        pch_schib_t *schib = get_schib(sid);
	int cc = do_sch_test(schib, &loc_scsw);
	*scsw = loc_scsw; // may be slow copy to flash
        trace_schib_scsw_cc(PCH_TRC_RT_CSS_SCH_TEST, schib,
                &loc_scsw, (uint8_t)cc);
	return cc;
}

static int do_sch_modify(pch_schib_t *schib, pch_pmcw_t pmcw) {
        int cc;
        uint32_t status = schibs_lock();

        if (schib_has_function_in_progress(schib)) {
                cc = 2;
                goto out;
        }

        if (schib_is_status_pending(schib)) {
                cc = 1;
                goto out;
        }

        assert(schib->mda.nextsid == get_sid(schib)); // shouldn't be on a list
	schib->pmcw.intparm = pmcw.intparm;
	schib->pmcw.flags = pmcw.flags & PCH_PMCW_SCH_MODIFY_MASK;
        cc = 0;

out:
        schibs_unlock(status);
	return cc;
}

int __time_critical_func(pch_sch_modify)(pch_sid_t sid, pch_pmcw_t *pmcw) {
	if (sid >= PCH_NUM_SCHIBS)
		return 3;

        pch_schib_t *schib = get_schib(sid);
	int cc = do_sch_modify(schib, *pmcw);
        trace_schib_byte(PCH_TRC_RT_CSS_SCH_MODIFY, schib, cc);
	return cc;
}

// caller must ensure *loc_schib is in fast RAM
static inline int do_sch_store(pch_schib_t *schib, pch_schib_t *loc_schib) {
        uint32_t status = schibs_lock();
	*loc_schib = *schib;
        schibs_unlock(status);
	return 0;
}

// caller must ensure size bytes at dst are in fast RAM and that size
// is at most sizeof(pch_schib_t)-offset
static inline int do_sch_store_partial(pch_schib_t *schib, void *dst, size_t size, size_t offset) {
        uint32_t status = schibs_lock();
        unsigned char *src = (unsigned char *)schib;
	memcpy(dst, src + offset, size);
        schibs_unlock(status);
	return 0;
}

int __time_critical_func(pch_sch_store)(pch_sid_t sid, pch_schib_t *out_schib) {
	pch_schib_t loc_schib; // must be on stack (fast RAM)
	if (sid >= PCH_NUM_SCHIBS)
		return 3;

        pch_schib_t *schib = get_schib(sid);
	int cc = do_sch_store(schib, &loc_schib);
	*out_schib = loc_schib; // may be slow copy

	trace_schib_byte(PCH_TRC_RT_CSS_SCH_STORE, schib, cc);
	return cc;
}

int __time_critical_func(pch_sch_store_pmcw)(pch_sid_t sid, pch_pmcw_t *out_pmcw) {
	pch_pmcw_t loc_pmcw; // must be on stack (fast RAM)
	if (sid >= PCH_NUM_SCHIBS)
		return 3;

        pch_schib_t *schib = get_schib(sid);
	int cc = do_sch_store_partial(schib, &loc_pmcw,
                sizeof(pch_pmcw_t), offsetof(pch_schib_t, pmcw));
	*out_pmcw = loc_pmcw; // may be slow copy

	trace_schib_byte(PCH_TRC_RT_CSS_SCH_STORE, schib, cc);
	return cc;
}

int __time_critical_func(pch_sch_store_scsw)(pch_sid_t sid, pch_scsw_t *out_scsw) {
	pch_scsw_t loc_scsw; // must be on stack (fast RAM)
	if (sid >= PCH_NUM_SCHIBS)
		return 3;

        pch_schib_t *schib = get_schib(sid);
	int cc = do_sch_store_partial(schib, &loc_scsw,
                sizeof(pch_scsw_t), offsetof(pch_schib_t, scsw));
	*out_scsw = loc_scsw; // may be slow copy

	trace_schib_byte(PCH_TRC_RT_CSS_SCH_STORE, schib, cc);
	return cc;
}

pch_intcode_t __time_critical_func(pch_test_pending_interruption)(void) {
	pch_schib_t *schib = pop_pending_schib();
        return css_make_intcode(schib);
}
