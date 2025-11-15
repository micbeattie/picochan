/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_CUS_CU_H
#define _PCH_CUS_CU_H

// PICO_CONFIG: PARAM_ASSERTIONS_ENABLED_PCH_CUS, Enable/disable assertions in the pch_cus module, type=bool, default=0, group=pch_cus
#ifndef PARAM_ASSERTIONS_ENABLED_PCH_CUS
#define PARAM_ASSERTIONS_ENABLED_PCH_CUS 0
#endif

#ifndef PCH_MAX_DEVIBS_PER_CU
#define PCH_MAX_DEVIBS_PER_CU 32
#endif

#include <stdint.h>
#include <assert.h>
#include "hardware/uart.h"
#include "picochan/dev_api.h"
#include "picochan/dmachan.h"
#include "txsm/txsm.h"

static_assert(__builtin_constant_p(PCH_MAX_DEVIBS_PER_CU),
        "PCH_MAX_DEVIBS_PER_CU must be a compile-time constant");

static_assert(PCH_MAX_DEVIBS_PER_CU >= 1 && PCH_MAX_DEVIBS_PER_CU <= 256,
        "PCH_MAX_DEVIBS_PER_CU must be between 1 and 256");

#define PCH_MAX_DEVIBS_PER_CU_ALIGN_SHIFT (31U - __builtin_clz(2 * (PCH_MAX_DEVIBS_PER_CU) - 1))

static_assert(__builtin_constant_p(PCH_MAX_DEVIBS_PER_CU_ALIGN_SHIFT),
        "__builtin_clz() did not produce compile-time constant for PCH_MAX_DEVIBS_PER_CU_ALIGN_SHIFT");

#define PCH_CU_ALIGN (1U << (PCH_DEVIB_SPACE_SHIFT+PCH_MAX_DEVIBS_PER_CU_ALIGN_SHIFT))

static_assert(__builtin_constant_p(PCH_CU_ALIGN),
        "could not produce compile-time constant for PCH_CU_ALIGN");

/*! \file picochan/cu.h
 *  \defgroup picochan_cu picochan_cu
 *
 * \brief Control Unit (CU)
 */

/*!
 * \def PCH_NUM_CUS
 * \ingroup picochan_cu
 * \hideinitializer
 * \brief The number of control units
 *
 * Must be a compile-time constant between 1 and 256. Default 4.
 * Defines the size of the global array of pch_cu_t structures
 * running on this Pico.
 */
#ifndef PCH_NUM_CUS
#define PCH_NUM_CUS 4
#endif
static_assert(PCH_NUM_CUS >= 1 && PCH_NUM_CUS <= 256,
        "PCH_NUM_CUS must be between 1 and 256");

#define PCH_CUS_BUFFERSET_MAGIC 0x70437553

/*! \brief pch_cu_t is a Control Unit (CU)
 *  \ingroup picochan_cu
 *
 * The struct starts with a fixed-size metadata section with state
 * and communication information about its devices and channel to
 * the CSS. Immediately following that (ignoring internal padding) is
 * an array of pch_devib_t structures, one for each device on the CU.
 * The size of that array is held in the num_devibs field of the
 * pch_cu_t which is set at the time pch_cu_init is called and
 * cannot be changed afterwards. The allocation of memory for a
 * pch_cu_t, whether static or dynamic, is the responsibility of the
 * application before calling pch_cu_init.
 *
 * The alignment of pch_cu_t is enforced to be PCH_CU_ALIGN which is
 * calculated at compile-time as PCH_MAX_DEVIBS_PER_CU multiplied by
 * the smallest power of 2 greater than or equal to
 * sizeof(pch_devib_t). This allows address arithmetic and bit masking
 * to determine the unit address and owning pch_cu_t of a devib.
 * PCH_MAX_DEVIBS_PER_CU, a preprocessor symbol, can be defined as any
 * compile-time constant between 1 and 256, defaulting to 32.
 * sizeof(pch_devib_t) is currently 16 so for the default
 * PCH_MAX_DEVIBS_PER_CU, alignof(pch_cu_t) is 512. With the
 * maximum PCH_MAX_DEVIBS_PER_CU of 256, alignof(pch_cu_t) is 4096.
 * Each individual pch_cu_t may be allocated at either compile-time or
 * runtime with a smaller numbers of devibs than PCH_MAX_DEVIBS_PER_CU
 * but the alignment as calculated above is still required.
 */
typedef struct __aligned(PCH_CU_ALIGN) pch_cu {
        dmachan_tx_channel_t    tx_channel;
        dmachan_rx_channel_t    rx_channel;
        pch_txsm_t              tx_pending;
        pch_cuaddr_t            cuaddr;
	//! when tx_pending in use, the ua to callback or -1
	int16_t                 tx_callback_ua;
	//! active ua for rx data to dev or -1 if none
	int16_t                 rx_active;
	//! head (active) ua on tx side or -1 if none
	int16_t                 tx_head;
	//! tail ua on tx side pending list of -1 if none
	int16_t                 tx_tail;
	//! completions raise irq dma.IRQ_BASE+dmairqix, -1 before configuration
	pch_dma_irq_index_t     dmairqix;
        uint8_t                 flags;
        uint16_t                num_devibs; //!< [0, 256]
        //! Flexible Array Member (FAM) of size num_devibs
	pch_devib_t             devibs[];
} pch_cu_t;

// values of pch_cu_t flags
#define PCH_CU_CONFIGURED       0x80
#define PCH_CU_STARTED          0x40
#define PCH_CU_TRACED_IRQ       0x04
#define PCH_CU_TRACED_LINK      0x02
#define PCH_CU_TRACED_GENERAL   0x01

// trace flags values start at the low bit
#define PCH_CU_TRACED_MASK      0x07

static inline bool pch_cu_is_configured(pch_cu_t *cu) {
        return cu->flags & PCH_CU_CONFIGURED;
}

static inline bool pch_cu_is_started(pch_cu_t *cu) {
        return cu->flags & PCH_CU_STARTED;
}

static inline uint8_t pch_cu_trace_flags(pch_cu_t *cu) {
        return cu->flags & PCH_CU_TRACED_MASK;
}

static inline bool pch_cu_is_traced_general(pch_cu_t *cu) {
        return cu->flags & PCH_CU_TRACED_GENERAL;
}

static inline bool pch_cu_is_traced_link(pch_cu_t *cu) {
        return cu->flags & PCH_CU_TRACED_LINK;
}

static inline bool pch_cu_is_traced_irq(pch_cu_t *cu) {
        return cu->flags & PCH_CU_TRACED_IRQ;
}

static inline pch_dma_irq_index_t pch_cu_get_dma_irq_index(pch_cu_t *cu) {
        return cu->dmairqix;
}

void pch_cu_set_dma_irq_index(pch_cu_t *cu, pch_dma_irq_index_t dmairqix);

/*! \def PCH_CU_INIT
 *  \ingroup picochan_cu
 *  \hideinitializer
 *  \brief a compile-time initialiser for a pch_cu_t
 *
 * PCH_CU_INIT relies on a non-standard C extension (supported by gcc)
 * to initialise a pch_cu_t that includes the space for its devibs
 * array (a Flexible Array Member) at the end of ths struct.
 * The num_devices macro argument is evaluated more than once but since
 * it must be a compile-time constant this should not be a problem.
 */
#define PCH_CU_INIT(num_devices) { \
                .tx_callback_ua = -1, \
                .rx_active = -1, \
                .tx_head = -1, \
                .tx_tail = -1, \
                .dmairqix = -1, \
                .num_devibs = (num_devices), \
                .devibs = { [(num_devices)-1] = {0} } \
        }

static inline pch_cu_t *pch_dev_get_cu(pch_devib_t *devib) {
        unsigned long p = (unsigned long)devib;
        p -= __builtin_offsetof(pch_cu_t, devibs);
        p &= ~(PCH_CU_ALIGN-1);
        return (pch_cu_t *)p;
}

static inline pch_cuaddr_t pch_dev_get_cuaddr(pch_devib_t *devib) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        return cu->cuaddr;
}

static inline pch_unit_addr_t pch_dev_get_ua(pch_devib_t *devib) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        return (pch_unit_addr_t)(devib - cu->devibs);
}

/*! \brief Look up the pch_devib_t of a device from its CU and unit address
 *  \ingroup picochan_cu
 *
 * This is a direct array member dereference into the devibs array
 * in the CU. There is no checking that ua is in range.
 */
static inline pch_devib_t *pch_get_devib(pch_cu_t *cu, pch_unit_addr_t ua) {
        return &cu->devibs[ua];
}

static inline bool cu_or_devib_is_traced(pch_devib_t *devib) {
        pch_cu_t *cu = pch_dev_get_cu(devib);
        return pch_cu_is_traced_general(cu) || pch_devib_is_traced(devib);
}

extern pch_cu_t *pch_cus[PCH_NUM_CUS];

extern bool pch_cus_init_done;

/*! \brief Get the CU for a given control unit address
 *  \ingroup picochan_cu
 *
 * For a Debug build, asserts when cua exceeds the
 * (compile-time defined) number of CUs, PCH_NUM_CUS, or if
 * the CU has not been initialised with pch_cu_init.
 */
static inline pch_cu_t *pch_get_cu(pch_cuaddr_t cua) {
        valid_params_if(PCH_CUS, cua < PCH_NUM_CUS);
        pch_cu_t *cu = pch_cus[cua];
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

/*
 * \brief Configure an explicit DMA IRQ for use by CUs started from the
 * calling core and set an exclusive IRQ handler for it.
 * \ingroup picochan_cu
 *
 * If a CSS is to be used on the same Pico, it must be initialised on
 * a different core, using a different DMA IRQ index. A convenient way
 * to still allow the CU subsystem to auto-configure its DMA IRQ
 * choice is to call pch_cus_ignore_dma_irq_index_t() on the DMA IRQ
 * index of the CSS.
*/
void pch_cus_configure_dma_irq_index_exclusive(pch_dma_irq_index_t dmairqix);

/*
 * \brief Configure an explicit DMA IRQ for use by CUs started from
 * the calling core and add a shared IRQ handler for it.
 * \ingroup picochan_cu
 *
 * If a CSS is to be used on the same Pico, it must be initialised on
 * a different core, using a different DMA IRQ index. A convenient way
 * to still allow the CU subsystem to auto-configure its DMA IRQ
 * choice is to call pch_cus_ignore_dma_irq_index_t() on the DMA IRQ
 * index of the CSS.
*/
void pch_cus_configure_dma_irq_index_shared(pch_dma_irq_index_t dmairqix, uint8_t order_priority);

/*
 * \brief Configure an explicit DMA IRQ for use by CUs started from
 * the calling core and add a shared IRQ handler for it using an
 * order_priority of PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY.
 * \ingroup picochan_cu
 *
 * If a CSS is to be used on the same Pico, it must be initialised on
 * a different core, using a different DMA IRQ index. A convenient way
 * to still allow the CU subsystem to auto-configure its DMA IRQ
 * choice is to call pch_cus_ignore_dma_irq_index_t() on the DMA IRQ
 * index of the CSS.
*/
void pch_cus_configure_dma_irq_index_shared_default(pch_dma_irq_index_t dmairqix);

/*
 * \brief Automatically choose and configure a suitable DMA IRQ for
 * use by CUs started from the calling core.
 * \ingroup picochan_cu
 *
 * If one of the explicit pch_cus_configure_dma_irq_index_...()
 * family of functions has already been called from the calling core
 * then the lowest such DMA IRQ index is returned. Otherwise, the
 * lowest DMA IRQ index is chosen that has not already been either
 * configured to any core or explicitly marked as not-to-use by
 * pch_cus_ignore_dma_irq_index_t(). It is then configured with
 * pch_cus_configure_dma_irq_index_shared_default() and returned.
 * In the case that no such unused index is available, the function
 * panics if required is true, otherwise -1 is returned.
 *
 * If a CSS is to be used on the same Pico, it must be initialised on
 * a different core, using a different DMA IRQ index. A convenient way
 * to still allow the CU subsystem to auto-configure its DMA IRQ
 * choice is to call pch_cus_ignore_dma_irq_index_t() on the DMA IRQ
 * index of the CSS.
*/
pch_dma_irq_index_t pch_cus_auto_configure_dma_irq_index(bool required);

/* \brief Marks dmairqix such that any call to
 * pch_cus_auto_configure_dma_irq_index(), whether explicit or
 * implicitly from pch_cu_start(), will not choose that DMA IRQ index.
 * \ingroup picochan_cu
 * 
 * This function is convenient for avoiding the need to configure
 * explicit DMA IRQ index numbers for the CU subsystem while ensuring
 * that its auto-configuration of DMA IRQ index numbers does not
 * conflict with those of a CSS in use on the same Pico or just some
 * other DMA IRQ index that needs to be reserved for application use.
 */
void pch_cus_ignore_dma_irq_index_t(pch_dma_irq_index_t dmairqix);

// CU initialisation and configuration

/*! \brief Initialises a CU with space for num_devibs devices.
 *  \ingroup picochan_cu
 *
 * \param cu Must be a pointer to enough space to hold the
 * pch_cu_t structure including its flexible array that must
 * itself have room for num_devibs pch_devib_t structures.
 * \param num_devibs The number of devices to initialise
 *
 * Typically, the PCH_CU_INIT macro is used as a static
 * initialiser instead of needing to call this function on an
 * uninitialised pch_cu_t.
 */
void pch_cu_init(pch_cu_t *cu, uint16_t num_devibs);

/*! \brief Registers a CU at a control unit address
 *  \ingroup picochan_cu
 *
 * \param cu the CU to register
 * \param cua control unit address to register as
 *
 * No CU must yet have been registered as control unit address cua.
 * cu must already have been initialised either with static
 * initialiser PCH_CU_INIT() or by calling pch_cu_init().
 */
void pch_cu_register(pch_cu_t *cu, pch_cuaddr_t cua);

/*! \brief Configure a UART control unit
 * \ingroup picochan_cu
 *
 * Configure the hardware UART instance uart as a channel from
 * CU cua to the CSS. The UART must have been initialised already,
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
 * can use pch_cus_auto_configure_uartcu() instead.
 */
void pch_cus_uartcu_configure(pch_cuaddr_t cua, uart_inst_t *uart, dma_channel_config ctrl);

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
void pch_cus_auto_configure_uartcu(pch_cuaddr_t cua, uart_inst_t *uart, uint baudrate);

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
 * pch_chp_get_tx_channel() must be used to fetch the internal
 * dmachan_tx_channel_t of the peer CSS channel for passing to
 * pch_cus_memcu_configure.
 */
void pch_cus_memcu_configure(pch_cuaddr_t cua, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dmachan_tx_channel_t *txpeer);

/*! \brief Starts the channel from CU cua to the CSS
 * \ingroup picochan_cu
 *
 * The CU must already have been registered by calling
 * pch_cu_register(). If the CU has already been started, this
 * function returns without doing anything. If no DMA IRQ index
 * has yet been explicitly configured for this CU then
 * pch_cus_auto_configure_dma_irq_index(true) is called and
 * pch_cu_set_dma_irq_index() is called to set the CU to use the
 * returned index. Then it marks the CU as started and starts the
 * channel to the CSS, allowing it to receive commands from the CSS.
 */
void pch_cu_start(pch_cuaddr_t cua);

/*! \brief Sets all/no trace flags for CU cua
 * \ingroup picochan_cu
 *
 * Sets all available CU trace flags (if trace is true) or unsets
 * all available CU trace flags (if trace is false) using
 * pch_cus_trace_cu(). Returns true if any trace flags were changed.
 */
bool pch_cus_trace_cu(pch_cuaddr_t cua, bool trace);

/*! \brief Sets what tracing flags are enabled for CU cua
 * \ingroup picochan_cu
 *
 * trace_flags must be a combination of zero or more of
 * PCH_CU_TRACED_GENERAL, PCH_CU_TRACED_LINK and PCH_CU_TRACED_IRQ.
 * If these flags do not include PCH_CU_TRACED_GENERAL then no CU
 * trace records are written for devices on this CU regardless of any
 * per-device trace flags.
 */
uint8_t pch_cu_set_trace_flags(pch_cuaddr_t cua, uint8_t trace_flags);

/*! \brief Sets whether tracing is enabled for device
 * \ingroup picochan_cu
 *
 * If this flag is set to true and the trace flag is set for the
 * CU subsystem as a whole (with pch_cus_set_trace) and the trace
 * flag is set for the device's CU (with pch_cus_trace_cu) then
 * device trace records are written for this device. If this
 * function changes the setting of the device's trace flag then a
 * trace record is written to indicate this (unlike using the
 * low-level pch_devib_set_traced() function).
 */
bool pch_cus_trace_dev(pch_devib_t *devib, bool trace);

// CU initialisation low-level helpers
void pch_cu_dma_configure(pch_cuaddr_t cua, dmachan_config_t *dc);
void pch_cu_set_configured(pch_cuaddr_t cua, bool configured);

/*! \brief Fetch the internal tx side of a channel from CU to CSS
 * \ingroup picochan_cu
 *
 * This function is only needed when configuring a memchan between
 * a CU and the CSS on different cores of a single Pico. The CSS
 * initialisation procedure uses this function to find its peer
 * CU structure in order to cross-connect the channels.
 */
dmachan_tx_channel_t *pch_cu_get_tx_channel(pch_cuaddr_t cua);

dmachan_rx_channel_t *pch_cu_get_rx_channel(pch_cuaddr_t cua);

void __isr pch_cus_handle_dma_irq(void);

typedef struct pch_dev_range {
        pch_cu_t        *cu;
        uint16_t        num_devices;    // 0 to 256
        pch_unit_addr_t first_ua;
} pch_dev_range_t;

static inline pch_unit_addr_t pch_dev_range_get_ua(pch_dev_range_t *dr, uint i) {
        assert(dr->cu);
        assert(i < dr->num_devices);
        assert((uint)dr->first_ua + i < dr->cu->num_devibs);

        return dr->first_ua + i;
}

static inline pch_unit_addr_t pch_dev_range_get_ua_required(pch_dev_range_t *dr, uint i) {
        if (!dr->cu)
                panic("missing cu in dev_range");

        if (i >= dr->num_devices)
                panic("index %lu not in dev_range", (unsigned long)i);

        return dr->first_ua + i;
}

static inline int pch_dev_range_get_index_nocheck(pch_dev_range_t *dr, pch_devib_t *devib) {
        return (int)pch_dev_get_ua(devib) - dr->first_ua;
}

static inline int pch_dev_range_get_index(pch_dev_range_t *dr, pch_devib_t *devib) {
        assert(dr->cu == pch_dev_get_cu(devib));

        int i = pch_dev_range_get_index_nocheck(dr, devib);
        if (i < 0 || i >= dr->num_devices)
                return -1;

        return i;
}

static inline int pch_dev_range_get_index_required(pch_dev_range_t *dr, pch_devib_t *devib) {
        int i = pch_dev_range_get_index(dr, devib);
        if (i < 0)
                panic("devib not found in dev_range");

        return i;
}

static inline pch_devib_t *pch_dev_range_get_devib_by_index(pch_dev_range_t *dr, uint i) {
        assert(dr->cu);

        pch_unit_addr_t ua = pch_dev_range_get_ua(dr, i);
        return pch_get_devib(dr->cu, ua);
}

static inline pch_devib_t *pch_dev_range_get_devib_by_index_required(pch_dev_range_t *dr, uint i) {
        pch_unit_addr_t ua = pch_dev_range_get_ua_required(dr, i);
        return pch_get_devib(dr->cu, ua);
}

static inline pch_devib_t *pch_dev_range_get_devib_by_ua_nocheck(pch_dev_range_t *dr, pch_unit_addr_t ua) {
        assert(dr->cu);

        return pch_get_devib(dr->cu, ua);
}

static inline pch_devib_t *pch_dev_range_get_devib_by_ua(pch_dev_range_t *dr, pch_unit_addr_t ua) {
        assert(dr->cu);

        if (ua < dr->first_ua
                || (uint)ua >= (uint)dr->first_ua + (uint)dr->num_devices) {
                return NULL;
        }

        return pch_get_devib(dr->cu, ua);
}

static inline pch_devib_t *pch_dev_range_get_devib_by_ua_required(pch_dev_range_t *dr, pch_unit_addr_t ua) {
        assert(dr->cu);

        if (ua < dr->first_ua
                || (uint)ua >= (uint)dr->first_ua + (uint)dr->num_devices) {
                panic("ua %u not in dev_range", ua);
        }

        return pch_get_devib(dr->cu, ua);
}

static inline int pch_dev_range_get_index_by_ua_nocheck(pch_dev_range_t *dr, pch_unit_addr_t ua) {
        return (int)ua - dr->first_ua;
}

static inline int pch_dev_range_get_index_by_ua(pch_dev_range_t *dr, pch_unit_addr_t ua) {
        int i = pch_dev_range_get_index_by_ua_nocheck(dr, ua);
        if (i < 0 || i >= dr->num_devices)
                return -1;

        return i;
}

static inline int pch_dev_range_get_index_by_ua_required(pch_dev_range_t *dr, pch_unit_addr_t ua) {
        int i = pch_dev_range_get_index_by_ua(dr, ua);
        if (i < 0)
                panic("ua %u not in dev_range", ua);

        return i;
}

static inline void pch_dev_range_init(pch_dev_range_t *drout, pch_cu_t *cu, pch_unit_addr_t first_ua, uint16_t num_devices) {
        assert(cu);
        assert((uint)first_ua + (uint)num_devices <= cu->num_devibs);

        drout->cu = cu;
        drout->num_devices = num_devices;
        drout->first_ua = first_ua;
}

static inline void pch_dev_range_set_callback(pch_dev_range_t *dr, pch_cbindex_t cbindex) {
        assert(dr->cu);

        for (uint i = 0; i < dr->num_devices; i++) {
                pch_devib_t *devib = pch_dev_range_get_devib_by_index(dr, i);
                pch_dev_set_callback(devib, cbindex);
        }
}

static inline pch_cbindex_t pch_dev_range_register_unused_devib_callback(pch_dev_range_t *dr, pch_devib_callback_t cb) {
        pch_cbindex_t cbindex = pch_register_unused_devib_callback(cb);
        pch_dev_range_set_callback(dr, cbindex);
        return cbindex;
}

#endif
