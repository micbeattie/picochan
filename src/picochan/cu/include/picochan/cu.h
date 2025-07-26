/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CUS_CU_H
#define _PCH_CUS_CU_H

// PICO_CONFIG: PARAM_ASSERTIONS_ENABLED_PCH_CUS, Enable/disable assertions in the pch_cus module, type=bool, default=0, group=pch_cus
#ifndef PARAM_ASSERTIONS_ENABLED_PCH_CUS
#define PARAM_ASSERTIONS_ENABLED_PCH_CUS 0
#endif

#ifndef PCH_MAX_DEVIBS_PER_CPU
#define PCH_MAX_DEVIBS_PER_CPU 32
#endif
static_assert(PCH_MAX_DEVIBS_PER_CPU >= 1 && PCH_MAX_DEVIBS_PER_CPU <= 256,
        "PCH_MAX_DEVIBS_PER_CPU must be between 1 and 256");

#include <stdint.h>
#include <assert.h>
#include "hardware/uart.h"
#include "picochan/dev_api.h"
#include "picochan/dmachan.h"
#include "txsm/txsm.h"

/*! \file picochan/cu.h
 *  \defgroup picochan_cu picochan_cu
 *
 * \brief Control Unit (CU)
 */

/*!
 * \def NUM_CUS
 * \ingroup picochan_cu
 * \hideinitializer
 * \brief The number of control units
 *
 * Must be a compile-time constant between 1 and 256. Default 4.
 * Defines the size of the global array of pch_cu_t structures
 * running on this Pico.
 */
#ifndef NUM_CUS
#define NUM_CUS 4
#endif
static_assert(NUM_CUS >= 1 && NUM_CUS <= 256,
        "NUM_CUS must be between 1 and 256");

#ifndef NUM_DEVIBS
#define NUM_DEVIBS 32
#endif
static_assert(NUM_DEVIBS >= 1 && NUM_DEVIBS <= 256,
        "NUM_DEVIBS must be between 1 and 256");

#define PCH_CUS_BUFFERSET_MAGIC 0x70437553

/*! \brief pch_cu_t is a Control Unit (CU)
 *  \ingroup picochan_cu
 *
 * The struct starts with a fixed-size metadata section with state
 * and communication information about its devices and channel to
 * the CSS. Immediately following that is an array of pch_devib_t
 * structures, one for each device on the CU. The size of that
 * array is held in the num_devibs field of the pch_cu_t which is
 * set at the time pch_cus_cu_init is called and cannot be changed
 * afterwards. The allocation of memory for a pch_cu_t, whether
 * static or dynamic, is the responsibility of the application
 * before calling pch_cus_cu_init.
 */
typedef struct __aligned(4) pch_cu {
        dmachan_tx_channel_t    tx_channel;
        dmachan_rx_channel_t    rx_channel;
        pch_txsm_t              tx_pending;
        pch_cunum_t             cunum;
	//! when tx_pending in use, the ua to callback or -1
	int16_t                 tx_callback_ua;
	//! active ua for rx data to dev or -1 if none
	int16_t                 rx_active;
	//! head (active) ua on tx side or -1 if none
	int16_t                 tx_head;
	//! tail ua on tx side pending list of -1 if none
	int16_t                 tx_tail;
	//! completions raise irq dma.IRQ_BASE+dmairqix
	uint8_t                 dmairqix;
        //! set to current core at start, verified at irq time
        int8_t                  corenum;
	bool                    traced;
	bool                    configured;
	bool                    started;
        uint16_t                num_devibs; //!< [0, 256]
        //! Flexible Array Member (FAM) of size num_devibs
	pch_devib_t             devibs[];
} pch_cu_t;

/*! \def PCH_CU_INIT
 *  \ingroup picochan_cu
 *  \hideinitializer
 *  \brief a compile-time initialiser for a pch_cu_t
 *
 * PCH_CU_INIT relies on a non-standard C extension (supported by gcc)
 * to initialise a pch_cu_t that includes the space for its devibs
 * array (a Flexible Array Member) at the end of ths struct.
 */
#define PCH_CU_INIT(num_devibs) {.devibs = { [num_devibs-1] = {0} }}

/*! \brief Look up the pch_devib_t of a device from its CU and unit address
 *  \ingroup picochan_cu
 *
 * This is a direct array member dereference into the devibs array
 * in the CU. There is no checking that ua is in range.
 */
static inline pch_devib_t *pch_get_devib(pch_cu_t *cu, pch_unit_addr_t ua) {
        return &cu->devibs[ua];
}

/*! \brief Look up the unit address of a device from its CU and devib
 *  \ingroup picochan_cu
 *
 * This is address arithmetic based on the knowledge that the devib
 * pointer lies within the fixed array of pch_devib_t structs within
 * the pch_cu_t structure.
 */
static inline pch_unit_addr_t pch_get_ua(pch_cu_t *cu, pch_devib_t *devib) {
        valid_params_if(PCH_CUS,
                devib >= &cu->devibs[0]
                && devib < &cu->devibs[NUM_DEVIBS]);
        return devib - cu->devibs;
}

static inline bool cu_or_devib_is_traced(pch_cu_t *cu, pch_devib_t *devib) {
        return cu->traced || pch_devib_is_traced(devib);
}

int16_t push_tx_list(pch_cu_t *cu, pch_unit_addr_t ua);

extern pch_cu_t *pch_cus[NUM_CUS];

extern bool pch_cus_init_done;

/*! \brief Get the CU for a given control unit number
 *  \ingroup picochan_cu
 *
 * For a Debug build, asserts when cunum exceeds the
 * (compile-time defined) number of CUs, NUM_CUS, or if
 * the CU has not been initialised with pch_cus_cu_init.
 */
static inline pch_cu_t *pch_get_cu(pch_cunum_t cunum) {
        valid_params_if(PCH_CUS, cunum < NUM_CUS);
        pch_cu_t *cu = pch_cus[cunum];
        assert(cu != NULL);
        return cu;
}

/*! \brief Initialise CU subsystem
 *  \ingroup picochan_cu
 *
 * Must be called before any other CU function.
 */
void pch_cus_init(void);

/*! \brief Sets whether CU subsystem tracing is enabled
 * \ingroup picochan_cu
 *
 * If this flag is not set to be true then no CU trace records are
 * written, regardless of any per-CU or per-device trace flags.
 */
bool pch_cus_set_trace(bool trace);

/*! \brief Initialise CU subsystem DMA interrupt handling using this DMA IRQ index
 * \ingroup picochan_cu
 *
 * Adds an IRQ handler and enables this DMA interrupt to be called on
 * the core that calls this function. The CU subsystem uses this
 * handler to drive its CU and device callback activity. If a CSS is
 * to be used on the same Pico, it must be initialised on a different
 * core, using a different DMA IRQ index.
*/
void pch_cus_init_dma_irq_handler(uint8_t dmairqix);

// CU initialisation and configuration

/*! \brief Initialises CU cunum with up to num_devibs devices.
 *  \ingroup picochan_cu
 *
 * \param cu Must be a pointer to enough space to hold the
 * pch_cu_t structure including its flexible array that must
 * itself have room for num_devibs pch_devib_t structures.
 * \param cunum control unit number to initialise
 * \param dmairqix Must be a DMA IRQ index on which
 * pch_cus_init_dma_irq_handler() has been invoked.
 * \param num_devibs The number of devices to initialise
 */
void pch_cus_cu_init(pch_cu_t *cu, pch_cunum_t cunum, uint8_t dmairqix, uint16_t num_devibs);

/*! \brief Configure a UART control unit
 * \ingroup picochan_cu
 *
 * Configure the hardware UART instance uart as a channel from
 * CU cunum to the CSS. The UART must have been initialised already,
 * be connected to the CSS using the same baud rate as the CSS has
 * configured and the hardware flow control pins, CTS and RTS MUST be
 * enabled and connected between CU and CSS.
 * ctrl should typically be a default dma_channel_config as returned
 * from dma_channel_get_default_config(dmaid) invoked on any DMA id.
 * Most bits in that dma_channel_config are overridden by the CU
 * (including the CHAIN_TO which is why the dmaid above does not
 * matter) but some applications may wish to set bits SNIFF_EN and
 * HIGH_PRIORITY for their own purposes.
 *
 * If you want to initialise and configure the UART channel using a
 * given baud rate, suggested UART settings (8E1) and default DMA
 * control register settings (no SNIFF_EN and no HIGH_PRIORITY), you
 * can use pch_cus_uartcu_init_and_configure() instead.
 */
void pch_cus_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl);

/*! \brief Initialise and configure a UART control unit with default
 * dma_channel_config control register.
 * \ingroup picochan_css
 *
 * Calls pch_uart_init() with baud rate \param baudrate and
 * pch_cus_uartcu_configure with ctrl argument bits taken from
 * an appropriate dma_channel_get_default_config() value.
 * The CSS on the other side of the channel MUST use the same baud
 * rate and uart settings set pch_uart_init().
 */
void pch_cus_uartcu_init_and_configure(pch_cunum_t cunum, uart_inst_t *uart, uint baudrate);

/*! \brief Configure a memchan control unit
 * \ingroup picochan_cu
 *
 * A memchan control unit allows the CU to run on one core of a
 * Pico while the CSS runs on the other core. Instead of using
 * physical pins or connections between CU and CSS, picochan uses the
 * DMA channels to copy memory-to-memory between CU and CSS and an
 * internal state machine and cross-core synchronisation to mediate
 * CU to CSS communications. txdmaid and rxdmaid must be two unused
 * DMA ids, typically allocated using dma_claim_unused_channel().
 * In order for the CU to find the CSS-side information to
 * cross-connect the sides in memory, the CSS API function
 * pch_css_cu_get_tx_channel() must be used to fetch the internal
 * dmachan_tx_channel_t of the peer CSS for passing to
 * pch_cus_memcu_configure.
 */
void pch_cus_memcu_configure(pch_cunum_t cunum, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer);

/*! \brief Starts the channel from CU cunum to the CSS
 * \ingroup picochan_cu
 *
 * The CU must be already configured but not have been started.
 * Marks the CU as started and starts the channel to the CSS,
 * allowing it to receive commands from the CSS.
 */
void pch_cus_cu_start(pch_cunum_t cunum);

/*! \brief Sets whether tracing is enabled for CU cunum
 * \ingroup picochan_cu
 *
 * If this flag is not set to be true then no CU trace records are
 * written for this CU and no device trace records, regardless
 * of any per-device trace flags.
 */
bool pch_cus_trace_cu(pch_cunum_t cunum, bool trace);

/*! \brief Sets whether tracing is enabled for device
 * \ingroup picochan_cu
 *
 * If this flag is set to true and the trace flag is set for the
 * CU subsystem as a whole (with pch_cus_set_trace) and the trace
 * flag is set for CU cnum (with pch_cus_trace_cu) then device
 * trace records are written for the device with unit address ua
 * on this CU.
 */
bool pch_cus_trace_dev(pch_cunum_t cunum, pch_unit_addr_t ua, bool trace);

// CU initialisation low-level helpers
void pch_cus_cu_dma_configure(pch_cunum_t cunum, dmachan_config_t *dc);
void pch_cus_cu_set_configured(pch_cunum_t cunum, bool configured);

/*! \brief Fetch the internal tx side of a channel from CU to CSS
 * \ingroup picochan_cu
 *
 * This function is only needed when configuring a memchan between
 * a CU and the CSS on different cores of a single Pico. The CSS
 * initialisation procedure uses this function to find its peer
 * CU structure in order to cross-connect the channels.
 */
dmachan_tx_channel_t *pch_cus_cu_get_tx_channel(pch_cunum_t cunum);

dmachan_rx_channel_t *pch_cus_cu_get_rx_channel(pch_cunum_t cunum);

void __isr pch_cus_handle_dma_irq(void);

/*! \brief Helper to call pch_dev_call_or_reject_then when the
 * caller has a devib but not the ua.
 *
 * This declaration would be more at home in dev_api.h but it needs
 * to know the size of pch_cu_t in order to do the address
 * arithmetic in pch_get_ua so it currently lives in cu.h instead.
 */
static inline int pch_dev_call_devib_or_reject_then(pch_cu_t *cu, pch_devib_t *devib, pch_dev_call_func_t f, int reject_cbindex_opt) {
        pch_unit_addr_t ua = pch_get_ua(cu, devib);
        return pch_dev_call_or_reject_then(cu, ua, f,
                reject_cbindex_opt);
}

/*! \brief Helper to call pch_dev_call_final_then when the
 * caller has a devib but not the ua.
 *
 * This declaration would be more at home in dev_api.h but it needs
 * to know the size of pch_cu_t in order to do the address
 * arithmetic in pch_get_ua so it currently lives in cu.h instead.
 */
static inline void pch_dev_call_devib_final_then(pch_cu_t *cu, pch_devib_t *devib, pch_dev_call_func_t f, int cbindex_opt) {
        pch_unit_addr_t ua = pch_get_ua(cu, devib);
        return pch_dev_call_final_then(cu, ua, f, cbindex_opt);
}
#endif
