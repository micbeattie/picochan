/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_API_CSS_H
#define _PCH_API_CSS_H

#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "pico/time.h"
#include "picochan/schib.h"
#include "picochan/ccw.h"
#include "picochan/intcode.h"
#include "picochan/dmachan.h"

/*! \file picochan/css.h
 *  \defgroup picochan_css picochan_css
 *
 * \brief Channel Subsystem (CSS)
 */

/*!
 * \def PCH_NUM_SCHIBS
 * \ingroup picochan_css
 * \hideinitializer
 * \brief The number of subchannels
 *
 * Must be a compile-time constant between 1 and 65536. Default 32.
 * Defines the size of the global array of schibs
 * (see \ref pch_schib_t).
 */
#ifndef PCH_NUM_SCHIBS
#define PCH_NUM_SCHIBS 32
#endif
static_assert(PCH_NUM_SCHIBS >= 1 && PCH_NUM_SCHIBS <= 65536,
        "PCH_NUM_SCHIBS must be between 1 and 65536");

/*!
 * \def PCH_NUM_CHANNELS
 * \ingroup picochan_css
 * \hideinitializer
 * \brief The number of channels that the CSS can use.
 *
 * Must be a compile-time constant between 1 and 256. Default 4.
 * One channel is needed to connect to each CU.
 * Defines the size of the global array of CSS-side channel
 * structures (see \ref pch_chp_t).
 */
#ifndef PCH_NUM_CHANNELS
#define PCH_NUM_CHANNELS 4
#endif
static_assert(PCH_NUM_CHANNELS >= 1 && PCH_NUM_CHANNELS <= 256,
        "PCH_NUM_CHANNELS must be between 1 and 256");

/*!
 * \def PCH_NUM_ISCS
 * \ingroup picochan_css
 * \hideinitializer
 * \brief The number of interrupt service classes.
 * \ingroup picochan_css
 *
 * Must be a compile-time constant between 1 and 8. Default 8.
 * Defines the size of the global array of linked-list-headers
 * for subchannels that are status pending.
 */
#ifndef PCH_NUM_ISCS
#define PCH_NUM_ISCS 8
#endif
static_assert(PCH_NUM_ISCS >= 1 && PCH_NUM_ISCS <= 8,
        "PCH_NUM_ISCS must be between 1 and 8");

#define PCH_CSS_BUFFERSET_MAGIC 0x70437353

/*! \brief A callback function to be invoked when a subchannel becomes status pending
 * \ingroup picochan_css
 */

typedef void(*io_callback_t)(pch_intcode_t, pch_scsw_t);

/*! \brief Get the addr field of a CCW as a pointer.
 * \ingroup picochan_css
 *
 * This is a convenience function that cannot be put in ccw.h itself
 * since the architected addr field is 32 bits and ccw.h must be
 * usable on platforms where a (void*) is longer without causing
 * compiler warnings (for example for compiling pch_dump_trace
 * off-platform).
 */
static inline void *pch_ccw_get_addr(pch_ccw_t ccw) {
        return (void *)ccw.addr;
}

/*! \brief Initialise CSS.
 *  \ingroup picochan_css
 *
 * Must be called before any other CSS function.
 */
void pch_css_init(void);

// Accessor functions for basic CSS settings
int8_t pch_css_get_core_num(void);
int8_t pch_css_get_dma_irq_index(void);
int16_t pch_css_get_func_irq(void);
int16_t pch_css_get_io_irq(void);

// A variety of different initialisation functions for configuring
// CSS IRQ numbers and handlers for DMA IRQ index, function IRQ
// and I/O IRQ.

void pch_css_set_dma_irq_index(pch_dma_irq_index_t dmairqix);
void pch_css_configure_dma_irq_index_shared(pch_dma_irq_index_t dmairqix, uint8_t order_priority);
void pch_css_configure_dma_irq_index_exclusive(pch_dma_irq_index_t dmairqix);
void pch_css_configure_dma_irq_index_default_shared(uint8_t order_priority);
void pch_css_configure_dma_irq_index_default_exclusive();
void pch_css_auto_configure_dma_irq_index();

/*! \brief Low-level function to set the IRQ number that the CSS uses
 * for application API notification to CSS
 * \ingroup picochan_css
 *
 * Typically, should be a non-externally-used user IRQ (i.e. IRQ
 * numbers 26-31 on RP2040 and IRQ numbers 46-51 (SPAREIRQ_IRQ0
 * through SPAREIRQ_IRQ5) on RP2350.
 * In general, either the high-level convenience function
 * pch_css_auto_configure_func_irq() should be used instead or,
 * for mid-level control of the handler, variants on
 * pch_css_configure_func_irq...
 */
void pch_css_set_func_irq(irq_num_t irqnum);
void pch_css_configure_func_irq_shared(irq_num_t irqnum, uint8_t order_priority);
void pch_css_configure_func_irq_exclusive(irq_num_t irqnum);
void pch_css_configure_func_irq_unused_shared(bool required, uint8_t order_priority);
void pch_css_configure_func_irq_unused_exclusive(bool required);
void pch_css_auto_configure_func_irq(bool required);

/*! \brief Low-level function to set the IRQ number that the CSS uses
 * for I/O interrupt notification.
 * \ingroup picochan_css
 *
 * Typically, should be a non-externally-used user IRQ (i.e. IRQ
 * numbers 26-31 on RP2040 and IRQ numbers 46-51 (SPAREIRQ_IRQ0
 * through SPAREIRQ_IRQ5) on RP2350.
 * In general, either the high-level convenience function
 * pch_css_auto_configure_io_irq() should be used instead or,
 * for mid-level control of the handler, variants on
 * pch_css_configure_io_irq...
 */
void pch_css_set_io_irq(irq_num_t irqnum);
void pch_css_configure_io_irq_shared(irq_num_t irqnum, uint8_t order_priority);
void pch_css_configure_io_irq_exclusive(irq_num_t irqnum);
void pch_css_configure_io_irq_unused_shared(bool required, uint8_t order_priority);
void pch_css_configure_io_irq_unused_exclusive(bool required);
void pch_css_auto_configure_io_irq(bool required);

/*! \brief Low-level function to set the I/O callback function that
 * the CSS invokes if its I/O interrupt handler has been set to
 * pch_css_io_irq_handler.
 * pch_css_start(io_callback, isc_mask) with io_callback non-NULL).
 * \ingroup picochan_css
 *
 * Typically, this should instead be set implicitly by calling
 * pch_css_start(io_callback, isc_mask) with io_callback non-NULL.
 */
io_callback_t pch_css_set_io_callback(io_callback_t io_callback);

/*! \brief Starts CSS operation after setting the io_callback
 * (if non-NULL), configuring and enabling any needed CSS
 * IRQ handlers that have not yet been set and setting the mask
 * of ISCs that trigger I/O interrupts to be isc_mask.
 * \ingroup picochan_css
 *
 * pch_css_init() must be called before calling this function.
 * If the CSS DMA IRQ index is not yet set, it is configured using
 * the index number corresponding to the current core number.
 * If the function IRQ is not set, it is configured by claiming an
 * unused user IRQ, setting the handler to pch_css_func_irq_handler
 * and enabling the IRQ. If io_callback is non-NULL then it is set as
 * the CSS io_callback function after, if the I/O IRQ is not set,
 * configuring it by claiming an unused IRQ, settting the handler to
 * pch_css_io_irq_handler and enabling the IRQ. Any IRQ handlers set
 * from this function are added using irq_add_shared_handler() with an
 * order_priority of PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY.
 */
void pch_css_start(io_callback_t io_callback, uint8_t isc_mask);

/*! \brief Sets whether CSS tracing is enabled
 * \ingroup picochan_css
 *
 * If this flag is not set to be true then no CSS trace records are
 * written, regardless of any per-channel or per-subchannel trace
 * flags.
 */
bool pch_css_set_trace(bool trace);

/*! \brief Sets what CSS trace events are enabled for channel chpid.
 * Flags may be a combination of PCH_CHP_TRACED_GENERAL,
 * PCH_CHP_TRACED_LINK, PCH_CHP_TRACED_IRQ.
 * Value PCH_CHP_TRACED_MASK is the set of all valid trace flags.
 * If these flags do not include PCH_CHP_TRACED_GENERAL then no
 * trace records are written for schibs using this channel regardless
 * of any per-schib trace flags.
 * Returns the old set of trace flags.
 * \ingroup picochan_css
 */
uint8_t pch_chp_set_trace_flags(pch_chpid_t chpid, uint8_t trace_flags);

#define PCH_CHP_TRACED_IRQ              0x04
#define PCH_CHP_TRACED_LINK             0x02
#define PCH_CHP_TRACED_GENERAL          0x01

#define PCH_CHP_TRACED_MASK             0x07

/*! \brief Uses pch_chp_set_trace_flags() to sets all available chpid
 * trace flags (if trace is true) or unsets all available chpid trace
 * flags (if trace is false). Returns true if any were changed.
 */
bool pch_chp_set_trace(pch_chpid_t chpid, bool trace);

void __isr pch_css_func_irq_handler(void);
void __isr pch_css_io_irq_handler(void);

/*! \brief Sets the IRQ number that the CSS raises when a subchannel becomes status pending
 * \ingroup picochan_css
 *
 * Typically, should be a non-externally used IRQ (i.e. IRQ numbers
 * 26-31 on RP2040 and IRQ numbers 46-51 (SPAREIRQ_IRQ0 through
 * SPAREIRQ_IRQ5) on RP2350. Although the application can use its own
 * ISR if it wishes, adding function pch_css_io_irq_handler as an ISR
 * for this interrupt lets the CSS itself handle callbacks for
 * subchannels with pending status (see \ref pch_css_set_io_callback).
 */
void pch_css_set_io_irq(irq_num_t irqnum);


/*! \brief Sets a callback function which pch_css_io_irq_handler will invoke on subchannels with unmasked ISC and pending status
 * \ingroup picochan_css
 *
 * If pch_css_io_irq_handler is added as an ISR for the
 * CSS I/O IRQ index (itself set with \ref pch_css_set_io_irq), then
 * when called, it pops each subchannel that is in an unmasked ISC
 * and is status pending, retrieves the SCSW for that subchannel and
 * calls the callback function.
 */
io_callback_t pch_css_set_io_callback(io_callback_t io_callback);

/*! \brief Starts channel chpid connection to its remote CU
 * \ingroup picochan_css
 *
 * The channel must be already configured but not have been started.
 * Marks the channel as started and starts it, allowing it to receive
 * commands from its remote CU.
 */
void pch_chp_start(pch_chpid_t chpid);

// Channel initialisation

/*! \brief Mark channel path chpid as claimed. Panics if it is
 * already claimed or allocated.
 * \ingroup picochan_css
 */
void pch_chp_claim(pch_chpid_t chpid);

/*! \brief Claims the next unclaimed and unallocated channel path and
 * returns its CHPID (a pch_chpid_t cast to int). If no channel path
 * is available, panics if required is true or else returns -1.
 * \ingroup picochan_css
 */
int pch_chp_claim_unused(bool required);

/*! \brief Allocates num_devices schibs for use by channel chpid.
 * \ingroup picochan_css
 *
 * Starting with the first unallocated schib in the CSS array of
 * schibs, allocates num_devices consecutive schibs and initialises
 * them to reference the devices with unit addresses 0 through
 * num_devices-1 respectively on the CU to which channel chpid will
 * connect. The total number of allocated schibs must not exceed the
 * size of the array, PCH_NUM_SCHIBS. A check for this and other
 * sanity checks on the arguments are made only if assertions are
 * enabled. CSS must have been started (\ref pch_css_start()) but
 * this channel must not have been started yet (\ref pch_chp_start()).
 * Returns the SID of the first allocated schib.
 */
pch_sid_t pch_chp_alloc(pch_chpid_t chpid, uint16_t num_devices);

/*! \brief Configure a UART channel
 * \ingroup picochan_css
 *
 * Configure the hardware UART instance uart as a channel to the
 * remote CU to which it is connected. This will initialise the  UART.
 * It must connected to a CU using the same baud rate as this channel
 * configures with cfg. The hardware flow control pins, CTS and RTS
 * *MUST* be enabled and connected between channel and CU.
 * Use pch_uartchan_get_default_config() to obtain a default
 * value for cfg and only make changes you need. You may well want
 * to change baudrate. For ctrl, the only bits you may want to change
 * are SNIFF_EN and HIGH_PRIORITY.
 */

void pch_chp_configure_uartchan(pch_chpid_t chpid, uart_inst_t *uart, pch_uartchan_config_t *cfg);

/*! \brief Configure a memchan channel
 * \ingroup picochan_css
 *
 * A memchan channel allows the CSS to run on one core of a
 * Pico while a CU runs on the other core. Instead of using physical
 * pins or connections between CU and CSS, picochan uses two DMA
 * channels to copy memory-to-memory between CSS and CU and an
 * internal state machine and cross-core synchronisation to mediate
 * CSS to CU communications.
 * In order for the CSS to find the CU-side information to
 * cross-connect the sides in memory, the CU API function
 * pch_cu_get_channel() must be used to fetch the internal
 * pch_channel_t of the peer CU for passing to
 * pch_chp_configure_memchan.
 */
void pch_chp_configure_memchan(pch_chpid_t chpid, pch_channel_t *chpeer);

// Channel initialisation low-level helpers
void pch_chp_dma_configure(pch_chpid_t chpid, dmachan_config_t *dc);
/*! \brief Get the underlying channel from a channel path from CSS to CU
 * \ingroup picochan_css
 *
 * This function is only needed when configuring a memchan between
 * a CSS and CU on different cores of a single Pico. The CU
 * initialisation procedure uses this function to find its peer
 * CSS structure in order to cross-connect the channels.
 */
pch_channel_t *pch_chp_get_channel(pch_chpid_t chpid);

// Architectural API for subchannels and channel programs

/*! \brief Start a channel program for a subchannel
 * \ingroup picochan_css
 *
 * Starts a channel program running for subchannel sid starting with
 * the CCW at address ccw_addr.
 *
 * The function updates an internal linked list and state then raises
 * an IRQ for the CSS to start the channel program asynchronously.
 * For a Release-build, the function will typically take dozens rather
 * than hundreds of CPU cycles.
 */
int pch_sch_start(pch_sid_t sid, pch_ccw_t *ccw_addr);

/*! \brief Resume a channel program for a subchannel
 * \ingroup picochan_css
 *
 * Resumes a channel program that has been started for subchannel sid
 * but has become suspended by reaching a CCW with the Suspend flag
 * (PCH_CCW_FLAG_S) set.
 *
 * The function updates an internal linked list and state then raises
 * an IRQ for the CSS to resume the channel program asynchronously.
 * For a Release-build, the function will typically take dozens rather
 * than hundreds of CPU cycles.
 */
int pch_sch_resume(pch_sid_t sid);

/*! \brief Test the status of a subchannel, clearing various status conditions of status is pending
 * \ingroup picochan_css
 *
 * Retrieves a SCSW representing the current status of a subchannel.
 * If the subchannel is "status pending", removes it from the list
 * of subchannels that are the cause of an I/O interruption condition
 * (or callback) and clears pending function conditions and, if set,
 * the "Suspended" condition.
 */
int pch_sch_test(pch_sid_t sid, pch_scsw_t *scsw);

/*! \brief Modifies the PMCW field of a subchannel
 * \ingroup picochan_css
 *
 * Only the following parts of the PMCW of the subchannel are modified
 * by this function; all other parts are ignored:
 *
 * * intparm
 * * flags bits in mask PCH_PMCW_SCH_MODIFY_MASK
 *
 * The bits in PCH_PMCW_SCH_MODIFY_MASK are PCH_PMCW_ENABLED,
 * PCH_PMCW_TRACED and the ISC bits, PCH_PMCW_ISC_BITS.
 */
int pch_sch_modify(pch_sid_t sid, pch_pmcw_t *pmcw);

/*! \brief Stores the contents of the schib for subchannel sid to out_schib
 * \ingroup picochan_css
 *
 * Although the schib may be in memory that is addressable by the
 * picochan CSS, it is architecturally independent and no part of
 * the CSS API relies on that. pch_sch_store is the architectural API
 * that provides access to the contents of the schib by copying it
 * from its internal location to the application-visible memory
 * pointed to by out_schib. The PMCW and SCSW parts of the schib are
 * architectural and can be relied on to be as documented. The rest
 * of the schib - the Model Dependent Area (MDA) - is intended to be
 * an internal implementation detail.
 */
int pch_sch_store(pch_sid_t sid, pch_schib_t *out_schib);

/*! \brief Cancel a channel program that has not yet started
 * \ingroup picochan_css
 *
 * pch_sch_cancel tries to cancel a channel program before it has
 * started. If pch_sch_cancel is called before the CSS has actually
 * started the channel program (meaning that pch_sch_start() has
 * set the AcStartPending in the subchannel's SCSW control flags
 * but the function IRQ handler that would then process the Start
 * has not yet run), then it cancels the start and returns condition
 * code 0. Otherwise, it returns 1 meaning "too late to cancel"
 * or 2 for "no such sid".
 *
 * pch_sch_cancel only acts on the schib; it does not trigger any
 * interrupt to cause any function IRQ not does it communicate with
 * the CU in any way.
 */
int pch_sch_cancel(pch_sid_t sid);

/*! \brief Halt a channel program
 * \ingroup picochan_css
 *
 * pch_sch_halt tries to halt a channel program. It sets the
 * subchannel's AcHaltPending flag and triggers a CSS function IRQ
 * which sends a Halt command to the CU for the device. The CU and
 * device driver are responsible for acting on the Halt command in
 * a timely manner and responding with an UpdateStatus to end the
 * channel program as soon as reasonably convenient. Depending on
 * the device driver, the Halt may or may not return a normal status. 
 */
int pch_sch_halt(pch_sid_t sid);

/*! \brief Test if there is a pending I/O interruption
 * \ingroup picochan_css
 *
 * If there is at least one subchannel which is "status pending"
 * with an interruption condition then pch_test_pending_interruption
 * returns an pch_intcode_t containing the sid of the subchannel,
 * its ISC, a condition code field of 1 and removes the subchannel
 * from the list of those with a pending I/O interruption condition.
 * If there is no such subchannel the condition code field of the
 * returned pch_intcode_t is 0.
 *
 * This function should only be called if the ISCs of any subchannels
 * that may become pending are masked or else there is a race
 * condition with any I/O ISR such as pch_css_io_irq_handler which
 * would process the I/O interruption itself.
 */
pch_intcode_t pch_test_pending_interruption(void);

// API additions with internal optimisation

/*! \brief Stores the contents of the PMCW part of the schib for subchannel sid to out_pmcw
 * \ingroup picochan_css
 *
 * This is a convenience/optimised subset of pch_sch_store that
 * only stores the PMCW part of the schib.
 */
int pch_sch_store_pmcw(pch_sid_t sid, pch_pmcw_t *out_pmcw);

/*! \brief Stores the contents of the SCSW part of the schib for subchannel sid to out_scsw
 * \ingroup picochan_css
 *
 * This is a convenience/optimised subset of pch_sch_store that
 * only stores the SCSW part of the schib.
 */
int pch_sch_store_scsw(pch_sid_t sid, pch_scsw_t *out_scsw);

// Convenience API functions that wrap the architectural API

/*! \brief Modifies the intparm field of the PMCW part of the schib for subchannel sid
 * \ingroup picochan_css
 *
 * This is a convenience/optimised subset of pch_sch_modify that
 * only modifies the Interruption Parameter of the subchannel.
 */
int pch_sch_modify_intparm(pch_sid_t sid, uint32_t intparm);

/*! \brief Modifies the flags field of the PMCW part of the schib for subchannel sid
 * \ingroup picochan_css
 *
 * This is a convenience/optimised subset of pch_sch_modify that
 * only modifies the PMCW flags of the subchannel.
 */
int pch_sch_modify_flags(pch_sid_t sid, uint16_t flags);

/*! \brief Modifies the isc field of the PMCW part of the schib for subchannel sid
 * \ingroup picochan_css
 *
 * This is a convenience/optimised subset of pch_sch_modify that
 * only modifies the ISC of the subchannel.
 */
int pch_sch_modify_isc(pch_sid_t sid, uint8_t isc);

/*! \brief Modifies enabled flag of the schib for subchannel sid
 * \ingroup picochan_css
 *
 * This is a convenience/optimised subset of pch_sch_modify that
 * only modifies the enabled flag of the subchannel.
 */
int pch_sch_modify_enabled(pch_sid_t sid, bool enabled);

/*! \brief Modifies traced flag of the schib for subchannel sid
 * \ingroup picochan_css
 *
 * This is a convenience/optimised subset of pch_sch_modify that
 * only modifies the traced flag of the subchannel.
 */
int pch_sch_modify_traced(pch_sid_t sid, bool traced);

/*! \brief Calls pch_sch_modify_isc() on count subchannels starting
 * from sid, panicking if any call fails
 * \ingroup picochan_css
 */
void __time_critical_func(pch_sch_modify_isc_range)(pch_sid_t sid, uint count, uint8_t isc);

/*! \brief Calls pch_sch_modify_enabled() on count subchannels
 * starting from sid, panicking if any call fails
 * \ingroup picochan_css
 */
void __time_critical_func(pch_sch_modify_enabled_range)(pch_sid_t sid, uint count, bool enabled);

/*! \brief Calls pch_sch_modify_traced() on count subchannels starting
 * from sid, panicking if any call fails
 * \ingroup picochan_css
 */
void __time_critical_func(pch_sch_modify_traced_range)(pch_sid_t sid, uint count, bool traced);

// These functions should only be called while the ISC for the
// subchannel has been disabled

/*! \brief Wait for an I/O interruption condition for subchannel sid
 * \ingroup picochan_css
 *
 * This is a convenience function which loops calling pch_sch_test
 * on the subchannel, returning with the fetched SCSW when the
 * subchannel becomes status pending. In between each call to
 * pch_sch_test, the function calls __wfe() since the subchannel
 * can only become status pending after the CSS processes an
 * interrupt.
 * This function must only be called while the ISC for the subchannel
 * is masked or else there is a race condition with any I/O ISR such
 * as pch_css_io_irq_handler which would process the I/O interruption
 * itself.
 * \returns Condition code - returned from pch_sch_test (will not be 1 since the function loops in this case)
 */
int pch_sch_wait(pch_sid_t sid, pch_scsw_t *scsw);

/*! \brief Wait for an I/O interruption condition for subchannel sid with a timeout
 * \ingroup picochan_css
 *
 * This is a convenience function which behaves the same as
 * pch_sch_wait except that it also returns if the timeout expires
 * (i.e. absolute time timeout_timestamp is reached) without the
 * subchannel having become status pending.
 */
int pch_sch_wait_timeout(pch_sid_t sid, pch_scsw_t *scsw, absolute_time_t timeout_timestamp);

/*! \brief Start a channel program for a subchannel and wait for an I/O interruption condition
 * \ingroup picochan_css
 *
 * This is a convenience function which calls pch_sch_start to start
 * a channel program for subchannel sid and then calls pch_sch_wait
 * to wait for it to become status pending.
 */
int pch_sch_run_wait(pch_sid_t sid, pch_ccw_t *ccw_addr, pch_scsw_t *scsw);

/*! \brief Start a channel program for a subchannel and wait for an I/O interruption condition with a timeout
 * \ingroup picochan_css
 *
 * This is a convenience function which calls pch_sch_start to start
 * a channel program for subchannel sid and then calls
 * pch_sch_wait_timeout to wait for it to become status pending or
 * for a timeout to expire.
 */
int pch_sch_run_wait_timeout(pch_sid_t sid, pch_ccw_t *ccw_addr, pch_scsw_t *scsw, absolute_time_t timeout_timestamp);

void pch_css_trace_write_user(pch_trc_record_type_t rt, void *data, uint8_t data_size);

#endif
