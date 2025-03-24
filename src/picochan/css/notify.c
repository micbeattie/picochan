/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "css_internal.h"
#include "css_trace.h"

void __time_critical_func(css_notify)(pch_schib_t *schib, uint8_t devs) {
        if (schib_is_status_pending(schib))
                return; // already pending; nothing to do

        schib->scsw.devs = devs;
        schib->scsw.ctrl_flags |= PCH_SC_PENDING;
        trace_schib_byte(PCH_TRC_RT_CSS_NOTIFY, schib, devs);
        push_to_isc_dlist(schib);
}

pch_schib_t __time_critical_func(*pop_pending_schib)() {
        // Only consider Isc lists which are enabled and non-empty
        uint8_t mask = CSS.isc_enable_mask & CSS.isc_status_mask;
        int ffs = __builtin_ffs(mask);
        if (ffs == 0)
                return NULL;

        // The highest priority ISC with a non-empty list is the bit
        // index of the lowest bit set in mask. __builtin_ffs returns
        // 0 for empty, else lowest-bit-index plus 1
        pch_schib_t *schib = pop_pending_schib_from_isc(ffs - 1);
        assert(schib != NULL);
        return schib;
}

static void callback_one_pending_schib(pch_schib_t *schib) {
        pch_scsw_t scsw = schib->scsw;
        pch_intcode_t ic = css_make_intcode(schib);
        css_clear_pending_subchannel(schib);

        if (CSS.io_callback) {
                trace_schib_callback(PCH_TRC_RT_CSS_IO_CALLBACK,
                        schib, &ic);
                CSS.io_callback(ic, scsw);
        }
}

static void callback_pending_schibs(void) {
        while (1) {
                pch_schib_t *schib = pop_pending_schib();
                if (!schib)
                        break;

                callback_one_pending_schib(schib);
        }
}

void __isr __time_critical_func(pch_css_io_irq_handler)(void) {
        uint irqnum = __get_current_exception() - VTABLE_FIRST_IRQ;
        // We use CSS.io_irqnum being -1 to mean there is no I/O IRQ
        // set in which case we shouldn't really get here. However,
        // the cast to uint below works for this case too by
        // ignoring the IRQ.
        if (irqnum != (uint)CSS.io_irqnum)
                return;

        irq_clear(irqnum);
        callback_pending_schibs();
}
