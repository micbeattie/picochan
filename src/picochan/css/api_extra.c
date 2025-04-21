/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "picochan/css.h"

// Convenience API functions that wrap the architectural API

// pch_sch_modify_intparm does a non-atomic store-then-modify of the
// schib's PMCW to modify its intparm.
int __time_critical_func(pch_sch_modify_intparm)(pch_sid_t sid, uint32_t intparm) {
        pch_schib_t schib;
        int cc = pch_sch_store(sid, &schib);
        if (cc)
                return cc;

        schib.pmcw.intparm = intparm;
        return pch_sch_modify(sid, &schib.pmcw);
}

// pch_sch_modify_flags does a non-atomic store-then-modify of the
// schib's PMCW to modify its flags. As in pch_sch_modify itself,
// bits in flags outside PCH_PMCW_SCH_MODIFY_MASK are silently
// ignored.
int __time_critical_func(pch_sch_modify_flags)(pch_sid_t sid, uint16_t flags) {
        pch_schib_t schib;
        int cc = pch_sch_store(sid, &schib);
        if (cc)
                return cc;

        schib.pmcw.flags = flags;
        return pch_sch_modify(sid, &schib.pmcw);
}

// pch_sch_modify_isc does a non-atomic store-then-modify of the
// schib's PMCW to modify the ISC bitfield in its flags. The isc
// argument is the ISC number (0-7) and is placed in the
// appropriate place within the flags. If bits in isc outside
// PCH_PMCW_ISC_BITS are set (i.e. if isc is greater than 7) then
// cc 3 is returned.
int __time_critical_func(pch_sch_modify_isc)(pch_sid_t sid, uint8_t isc) {
        if (isc > PCH_PMCW_ISC_BITS)
                return 3;

        pch_schib_t schib;
        int cc = pch_sch_store(sid, &schib);
        if (cc)
                return cc;

        uint16_t flags = schib.pmcw.flags & ~PCH_PMCW_ISC_BITS;
        schib.pmcw.flags = flags | (isc << PCH_PMCW_ISC_LSB);
        return pch_sch_modify(sid, &schib.pmcw);
}

// pch_sch_modify_enabled does a non-atomic store-then-modify of the
// schib's PMCW to modify the enabled bit in its flags.
int __time_critical_func(pch_sch_modify_enabled)(pch_sid_t sid, bool enabled) {
        pch_schib_t schib;
        int cc = pch_sch_store(sid, &schib);
        if (cc)
                return cc;

        if (enabled)
                schib.pmcw.flags |= 1 << PCH_PMCW_ENABLED;
        else
                schib.pmcw.flags &= ~(1 << PCH_PMCW_ENABLED);

        return pch_sch_modify(sid, &schib.pmcw);
}

// pch_sch_modify_traced does a non-atomic store-then-modify of the
// schib's PMCW to modify the traced bit in its flags.
int __time_critical_func(pch_sch_modify_traced)(pch_sid_t sid, bool traced) {
        pch_schib_t schib;
        int cc = pch_sch_store(sid, &schib);
        if (cc)
                return cc;

        if (traced)
                schib.pmcw.flags |= 1 << PCH_PMCW_TRACED;
        else
                schib.pmcw.flags &= ~(1 << PCH_PMCW_TRACED);

        return pch_sch_modify(sid, &schib.pmcw);
}

int __time_critical_func(pch_sch_wait)(pch_sid_t sid, pch_scsw_t *scsw) {
        while (1) {
                int cc = pch_sch_test(sid, scsw);
                if (cc != 1)
                        return cc;

                __wfe();
        }

        // NOTREACHED
}

int __time_critical_func(pch_sch_wait_timeout)(pch_sid_t sid, pch_scsw_t *scsw, absolute_time_t timeout_timestamp) {
        while (1) {
                int cc = pch_sch_test(sid, scsw);
                if (cc != 1)
                        return cc;

                if (best_effort_wfe_or_timeout(timeout_timestamp))
                        return 2;
        }

        // NOTREACHED
}

int __time_critical_func(pch_sch_run_wait)(pch_sid_t sid, pch_ccw_t *ccw_addr, pch_scsw_t *scsw) {
        int cc = pch_sch_start(sid, ccw_addr);
        if (cc)
                return cc;

        return pch_sch_wait(sid, scsw);
}

int __time_critical_func(pch_sch_run_wait_timeout)(pch_sid_t sid, pch_ccw_t *ccw_addr, pch_scsw_t *scsw, absolute_time_t timeout_timestamp) {
        int cc = pch_sch_start(sid, ccw_addr);
        if (cc)
                return cc;

        return pch_sch_wait_timeout(sid, scsw, timeout_timestamp);
}
