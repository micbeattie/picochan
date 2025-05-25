/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_TRC_RECORDS_H
#define _PCH_API_TRC_RECORDS_H

#include "picochan/ids.h"
#include "picochan/ccw.h"
#include "picochan/css.h"

// Common structs for the data parts of trace records

struct pch_trdata_byte {
        uint8_t         byte;
};

struct pch_trdata_dev {
        pch_cunum_t     cunum;
        pch_unit_addr_t ua;
};

struct pch_trdata_cu_init {
        uint16_t        num_devices;
        pch_cunum_t     cunum;
        uint8_t         dmairqix;
};

struct pch_trdata_cu_irq {
        pch_cunum_t     cunum;
        uint8_t         dmairqix;
        uint8_t         tx_state;
        uint8_t         rx_state;
};

struct pch_trdata_cu_byte {
        pch_cunum_t     cunum;
        uint8_t         byte;
};

struct pch_trdata_dev_byte {
        pch_cunum_t     cunum;
        pch_unit_addr_t ua;
        uint8_t         byte;
};

struct pch_trdata_word_dev {
        uint32_t        word;
        pch_cunum_t     cunum;
        pch_unit_addr_t ua;
};

struct pch_trdata_word_sid_byte {
        uint32_t        word;
        pch_sid_t       sid;
        uint8_t         byte;
};

struct pch_trdata_word_byte {
        uint32_t        word;
        uint8_t         byte;
};

struct pch_trdata_word_sid {
        uint32_t        word;
        pch_sid_t       sid;
};

struct pch_trdata_sid_byte {
        pch_sid_t       sid;
        uint8_t         byte;
};

struct pch_trdata_ccw_addr_sid {
        pch_ccw_t       ccw;
        uint32_t        addr;
        pch_sid_t       sid;
};

struct pch_trdata_intcode_scsw {
        pch_intcode_t   intcode;
        pch_scsw_t      scsw;
};

struct pch_trdata_scsw_sid_cc {
        pch_scsw_t      scsw;
        pch_sid_t       sid;
        uint8_t         cc;
};

struct pch_trdata_cu_dma {
        uint32_t                addr;
        uint32_t                ctrl;
        pch_cunum_t             cunum;
        pch_dmaid_t             dmaid;
        pch_dma_irq_index_t     dmairqix;
};

struct pch_trdata_css_cu_claim {
        pch_sid_t       first_sid;
        uint16_t        num_devices;
        pch_cunum_t     cunum;
};

struct pch_trdata_irqnum_opt {
        int16_t         irqnum_opt;
};

struct pch_trdata_address_change {
        uint32_t        old_addr;
        uint32_t        new_addr;
};

struct pch_trdata_func_irq {
        int16_t         ua_opt;
        pch_cunum_t     cunum;
        uint8_t         tx_active;
};

#endif
