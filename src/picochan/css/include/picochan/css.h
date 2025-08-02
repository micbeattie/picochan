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
 * \def PCH_NUM_CSS_CUS
 * \ingroup picochan_css
 * \hideinitializer
 * \brief The number of control units that the CSS can use.
 *
 * Must be a compile-time constant between 1 and 256. Default 4.
 * Defines the size of the global array of CSS-side CU structures
 * (see \ref css_cu_t).
 */
#ifndef PCH_NUM_CSS_CUS
#define PCH_NUM_CSS_CUS 4
#endif
static_assert(PCH_NUM_CSS_CUS >= 1 && PCH_NUM_CSS_CUS <= 256,
        "PCH_NUM_CSS_CUS must be between 1 and 256");

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

/*! \brief Initialise CSS.
 *  \ingroup picochan_css
 *
 * Must be called before any other CSS function.
 */
void pch_css_init(void);

/*! \brief Initialise CSS DMA interrupt handling using this DMA IRQ index
 * \ingroup picochan_css
 *
 * Adds an IRQ handler and enables this DMA interrupt to be called on
 * the core that calls this function. The CSS uses this handler to drive
 * its channel program, CU and callback activity. If a CU is to be used
 * on the same Pico, it must be initialised on a different core, using a
 * different DMA IRQ index.
 */
void pch_css_start(uint8_t dmairqix);

/*! \brief Sets the IRQ number that the CSS uses for API notification to CSS
 * \ingroup picochan_css
 *
 * Typically, should be a non-externally used IRQ (i.e. IRQ numbers
 * 26-31 on RP2040 and IRQ numbers 46-51 (SPAREIRQ_IRQ0 through
 * SPAREIRQ_IRQ5) on RP2350.
 */
void pch_css_set_func_irq(irq_num_t irqnum);

/*! \brief Sets whether CSS tracing is enabled
 * \ingroup picochan_css
 *
 * If this flag is not set to be true then no CSS trace records are
 * written, regardless of any per-CU or per-subchannel trace flags.
 */
bool pch_css_set_trace(bool trace);

/*! \brief Sets whether CSS tracing is enabled for CU cunum
 * \ingroup picochan_css
 *
 * If this flag is not set to be true then no CU trace records are
 * written for this CU and no subchannel trace records, regardless
 * of any per-subchannel trace flags.
 */
bool pch_css_set_trace_cu(pch_cunum_t cunum, bool trace);

void __isr pch_css_schib_func_irq_handler(void);
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

/*! \brief Starts the channel to CU cunum
 * \ingroup picochan_css
 *
 * The CU must be already configured but not have been started.
 * Marks the CU as started and starts the channel to it, allowing
 * it to receive commands from the CU.
 */
void pch_css_cu_start(pch_cunum_t cunum);

// CSS CU initialisation

/*! \brief Mark CU cunum as claimed. Panics if the CU is already
 * claimed or allocated.
 * \ingroup picochan_css
 */
void pch_css_cu_claim(pch_cunum_t cunum);

/*! \brief Claims the next unclaimed and unallocated CU and returns
 * its CU number. If no CU is available, panics if required is true
 * or else returns -1.
 * \ingroup picochan_css
 */
int pch_css_cu_claim_unused(bool required);

/*! \brief Initialises num_devices schibs for use by CU cunum.
 * \ingroup picochan_css
 *
 * Starting with the first uninitialised schib in the CSS array of
 * schibs, allocates num_devices consecutive schibs and initialises
 * them to reference the devices with unit addresses 0 through
 * num_devices-1 respectively on CU cunum. The total number of
 * allocated schibs must not exceed the size of the array,
 * PCH_NUM_SCHIBS. A check for this and other sanity checks on the
 * arguments are made only if assertions are enabled. CSS must have
 * been started (pch_css_start()) but a channel to this CU must not
 * have been started yet (pch_css_cu_start()).
 * Returns the SID of the first allocated schib.
 */
pch_sid_t pch_css_cu_init(pch_cunum_t cunum, uint16_t num_devices);

/*! \brief Configure a UART control unit
 * \ingroup picochan_css
 *
 * Configure the hardware UART instance uart as a channel to CU cunum.
 * The UART must have been initialised already, be connected to a CU
 * using the same baud rate as the CU has configured and the hardware
 * flow control pins, CTS and RTS MUST be enabled and connected
 * between CSS and CU.
 * ctrl should typically be a default dma_channel_config as returned
 * from dma_channel_get_default_config(dmaid) invoked on any DMA id.
 * Most bits in that dma_channel_config are overridden by the CSS
 * (including the CHAIN_TO which is why the dmaid above does not
 * matter) but some applications may wish to set bits SNIFF_EN and
 * HIGH_PRIORITY for their own purposes.
 *
 * If you want to initialise and configure the UART channel using a
 * given baud rate, suggested UART settings (8E1) and default DMA
 * control register settings (no SNIFF_EN and no HIGH_PRIORITY), you
 * can use pch_css_uartcu_init_and_configure() instead.
 */

void pch_css_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl);

/*! \brief Initialise and configure a UART channel to a control unit
 * with default dma_channel_config control register.
 * \ingroup picochan_css
 *
 * Calls pch_uart_init() with baud rate \param baudrate and
 * pch_css_uartcu_configure with ctrl argument bits taken from
 * an appropriate dma_channel_get_default_config() value.
 * The CU on the other side of the channel MUST use the same baud
 * rate and uart settings set pch_uart_init().
 */
void pch_css_init_uartchan(pch_cunum_t cunum, uart_inst_t *uart, uint baudrate);

/*! \brief Configure a memchan control unit
 * \ingroup picochan_css
 *
 * A memchan control unit allows the CSS to run on one core of a
 * Pico while a CU runs on the other core. Instead of using physical
 * pins or connections between CU and CSS, picochan uses the DMA
 * channels to copy memory-to-memory between CSS and CU and an
 * internal state machine and cross-core synchronisation to mediate
 * CSS to CU communications. txdmaid and rxdmaid must be two unused
 * DMA ids, typically allocated using dma_claim_unused_channel().
 * In order for the CSS to find the CU-side information to
 * cross-connect the sides in memory, the CU API function
 * pch_cus_cu_get_tx_channel() must be used to fetch the internal
 * dmachan_tx_channel_t of the peer CU for passing to
 * pch_css_memcu_configure.
 */
void pch_css_memcu_configure(pch_cunum_t cunum, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer);

// CSS CU initialisation low-level helpers
void pch_css_cu_dma_configure(pch_cunum_t cunum, dmachan_config_t *dc);
void pch_css_cu_set_configured(pch_cunum_t cunum, bool configured);
/*! \brief Fetch the internal tx side of a channel from CSS to CU
 * \ingroup picochan_css
 *
 * This function is only needed when configuring a memchan between
 * a CSS and CU on different cores of a single Pico. The CU
 * initialisation procedure uses this function to find its peer
 * CSS structure in order to cross-connect the channels.
 */
dmachan_tx_channel_t *pch_css_cu_get_tx_channel(pch_cunum_t cunum);
dmachan_rx_channel_t *pch_css_cu_get_rx_channel(pch_cunum_t cunum);

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
 * pch_sch_start marks a subchannel as "Start Pending", adds it to
 * an internal list and raises an IRQ for the CSS to process
 * asynchronously. If pch_sch_cancel is called before the CSS has
 * actually started the channel program then it cancels the start.
 * \return Condition Code - 0 for cancelled, 1 for too late to cancel, 2 for no such sid
 */
int pch_sch_cancel(pch_sid_t sid);

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

#endif
