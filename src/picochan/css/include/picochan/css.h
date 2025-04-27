/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_CSS_H
#define _PCH_API_CSS_H

#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "pico/time.h"
#include "picochan/schib.h"
#include "picochan/dmachan.h"

#ifndef PCH_NUM_SCHIBS
#define PCH_NUM_SCHIBS 32
#endif
static_assert(PCH_NUM_SCHIBS >= 1 && PCH_NUM_SCHIBS <= 65536,
        "PCH_NUM_SCHIBS must be between 1 and 65536");

#ifndef PCH_NUM_CSS_CUS
#define PCH_NUM_CSS_CUS 4
#endif
static_assert(PCH_NUM_CSS_CUS >= 1 && PCH_NUM_CSS_CUS <= 256,
        "PCH_NUM_CSS_CUS must be between 1 and 256");

#ifndef PCH_NUM_ISCS
#define PCH_NUM_ISCS 8
#endif
static_assert(PCH_NUM_ISCS >= 1 && PCH_NUM_ISCS <= 8,
        "PCH_NUM_ISCS must be between 1 and 8");

#define PCH_CSS_BUFFERSET_MAGIC 0x70437353

typedef struct pch_schib pch_schib_t;

// An I/O interruption code is returned from pch_test_pending_interruption.
// (The original expansion of the acronym SID is
// Subsystem-Identification Word which is 32 bits and includes some bits of
// data beyond just the subchannel number. For Picochan we only use the
// 16-bit subchannel number so calling this the SID is more approriate.)
//
// pch_intcode_t
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |               Interruption Parameter (Intparm)                |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//         |  Subchannel ID (SID)          |      ISC      |           |cc |
//         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// The cc is the condition code which, for a return from
// pch_test_pending_interruption, only uses two values: 0 means there was
// no interrupt pending and the rest of the pch_intcode_t is meaningless;
// 1 means an interrupt was pending and its information has been returned.
typedef struct pch_intcode {
        uint32_t        intparm;
        pch_sid_t       sid;
        uint8_t         flags;
        uint8_t         cc;
} pch_intcode_t;
static_assert(sizeof(pch_intcode_t) == 8,
        "architected pch_intcode_t is 8 bytes");

typedef void(*io_callback_t)(pch_intcode_t, pch_scsw_t);

void pch_css_init(void);
void pch_css_start(uint8_t dmairqix);
void pch_css_set_func_irq(irq_num_t irqnum);
bool pch_css_set_trace(bool trace);
bool pch_css_set_trace_cu(pch_cunum_t cunum, bool trace);
void __isr pch_css_schib_func_irq_handler(void);
void __isr pch_css_io_irq_handler(void);
void pch_css_set_io_irq(irq_num_t irqnum);
io_callback_t pch_css_set_io_callback(io_callback_t io_callback);
void pch_css_start_channel(pch_cunum_t cunum);

// CSS CU initialisation
void pch_css_cu_claim(pch_cunum_t cunum, uint16_t num_devices);
void pch_css_cu_dma_configure(pch_cunum_t cunum, dmachan_config_t *dc);
void pch_css_uartcu_dma_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl);
// CSS CU initialisation low-level helpers
dma_channel_config pch_css_uartcu_make_rxctrl(uart_inst_t *uart, dma_channel_config ctrl);
dma_channel_config pch_css_uartcu_make_txctrl(uart_inst_t *uart, dma_channel_config ctrl);

// Architectural API for subchannels and channel programs
int pch_sch_start(pch_sid_t sid, pch_ccw_t *ccw_addr);
int pch_sch_resume(pch_sid_t sid);
int pch_sch_test(pch_sid_t sid, pch_scsw_t *scsw);
int pch_sch_modify(pch_sid_t sid, pch_pmcw_t *pmcw);
int pch_sch_store(pch_sid_t sid, pch_schib_t *out_schib);
int pch_sch_cancel(pch_sid_t sid);
pch_intcode_t pch_test_pending_interruption(void);

// API additions with internal optimisation
int pch_sch_store_pmcw(pch_sid_t sid, pch_pmcw_t *out_pmcw);
int pch_sch_store_scsw(pch_sid_t sid, pch_scsw_t *out_scsw);

// Convenience API functions that wrap the architectural API
int pch_sch_modify_intparm(pch_sid_t sid, uint32_t intparm);
int pch_sch_modify_flags(pch_sid_t sid, uint16_t flags);
int pch_sch_modify_isc(pch_sid_t sid, uint8_t isc);
int pch_sch_modify_enabled(pch_sid_t sid, bool enabled);
int pch_sch_modify_traced(pch_sid_t sid, bool traced);

// These functions should only be called while the ISC for the
// subchannel has been disabled
int pch_sch_wait(pch_sid_t sid, pch_scsw_t *scsw);
int pch_sch_wait_timeout(pch_sid_t sid, pch_scsw_t *scsw, absolute_time_t timeout_timestamp);
int pch_sch_run_wait(pch_sid_t sid, pch_ccw_t *ccw_addr, pch_scsw_t *scsw);
int pch_sch_run_wait_timeout(pch_sid_t sid, pch_ccw_t *ccw_addr, pch_scsw_t *scsw, absolute_time_t timeout_timestamp);

#endif
