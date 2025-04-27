/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_CUS_CU_H
#define _PCH_CUS_CU_H

// PICO_CONFIG: PARAM_ASSERTIONS_ENABLED_PCH_CUS, Enable/disable assertions in the pch_cus module, type=bool, default=0, group=pch_cus
#ifndef PARAM_ASSERTIONS_ENABLED_PCH_CUS
#define PARAM_ASSERTIONS_ENABLED_PCH_CUS 0
#endif

#include <stdint.h>
#include <assert.h>
#include "hardware/uart.h"
#include "picochan/dev_api.h"
#include "picochan/dmachan.h"
#include "txsm/txsm.h"
#include "trc/trace.h"

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

typedef struct __aligned(4) pch_cu {
        dmachan_tx_channel_t    tx_channel;
        dmachan_rx_channel_t    rx_channel;
        pch_txsm_t              tx_pending;
        pch_cunum_t             cunum;
	// tx_callback_ua: when tx_pending in use, the ua to callback or -1
	int16_t                 tx_callback_ua;
	// rx_active: active ua for rx data to dev or -1 if none
	int16_t                 rx_active;
	// tx_head: head (active) ua on tx side or -1 if none
	int16_t                 tx_head;
	// tx_tail: tail ua on tx side pending list of -1 if none
	int16_t                 tx_tail;
	// dmairqix: completions raise irq dma.IRQ_BASE+dmairqix
	uint8_t                 dmairqix;
	bool                    traced;
	bool                    enabled;
        uint16_t                num_devibs; // [0, 256]
	pch_devib_t             devibs[];
} pch_cu_t;

// PCH_CU_INIT relies on a non-standard C extension (supported by gcc)
// to initialise a pch_cu_t that includes the space for its devibs
// array (a Flexible Array Member) at the end of ths struct.
#define PCH_CU_INIT(num_devibs) {.devibs = { [num_devibs-1] = {0} }}

static inline pch_devib_t *pch_get_devib(pch_cu_t *cu, pch_unit_addr_t ua) {
        return &cu->devibs[ua];
}

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
void send_command_to_css(pch_cu_t *cu);

//
// Global array of CUs
//

extern pch_cu_t *pch_cus[NUM_CUS];

static inline pch_cu_t *pch_get_cu(pch_cunum_t cunum) {
        valid_params_if(PCH_CUS, cunum < NUM_CUS);
        pch_cu_t *cu = pch_cus[cunum];
        assert(cu != NULL);
        return cu;
}

void pch_cus_init(void);
bool pch_cus_set_trace(bool trace);
void pch_cus_init_dma_irq_handler(uint8_t dmairqix);
void pch_cus_register_cu(pch_cu_t *cu, pch_cunum_t cunum, uint8_t dmairqix, uint16_t num_devibs);
void pch_cus_cu_dma_configure(pch_cunum_t cunum, dmachan_config_t *dc);
void pch_cus_cu_dma_claim_and_configure(pch_cunum_t cunum, uint32_t txhwaddr, dma_channel_config txctrl, uint32_t rxhwaddr, dma_channel_config rxctrl);
void pch_cus_init_mem_channel(pch_cunum_t cunum, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid);
void pch_cus_enable_cu(pch_cunum_t cunum);
bool pch_cus_trace_cu(pch_cunum_t cunum, bool trace);
bool pch_cus_trace_dev(pch_cunum_t cunum, pch_unit_addr_t ua, bool trace);

// Convenience function for initialising CU
void pch_cus_uartcu_configure(pch_cunum_t cunum, uart_inst_t *uart, pch_dmaid_t txdmaid, pch_dmaid_t rxdmaid, dma_channel_config ctrl);
void pch_cus_uartcu_claim_and_configure(pch_cunum_t cunum, uart_inst_t *uart, dma_channel_config ctrl);

void __isr pch_cus_handle_dma_irq(void);

#endif
