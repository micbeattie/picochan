/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

#ifndef _PCH_API_TRC_RECORDS_H
#define _PCH_API_TRC_RECORDS_H

#include "picochan/ids.h"
#include "picochan/ccw.h"
#include "picochan/scsw.h"
#include "picochan/intcode.h"

// Common structs for the data parts of trace records

struct pch_trdata_byte {
        uint8_t         byte;
};

struct pch_trdata_id_byte {
        uint8_t         id;
        uint8_t         byte;
};

struct pch_trdata_irq_handler {
        uint32_t        handler;
        int16_t         order_priority; // -1 for exclusive
        uint8_t         irqnum;
};

struct pch_trdata_cu_register {
        uint16_t                num_devices;
        pch_cuaddr_t            cuaddr;
};

struct pch_trdata_id_irq {
        uint8_t         id;
        pch_irq_index_t irq_index;
        uint8_t         tx_state;
        uint8_t         rx_state;
};

struct pch_trdata_dev {
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
};

struct pch_trdata_dev_byte {
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
        uint8_t         byte;
};

struct pch_trdata_counts_dev {
        uint16_t        count1;
        uint16_t        count2;
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
};

struct pch_trdata_count_dev {
        uint16_t        count;
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
};

struct pch_trdata_packet_dev {
        uint32_t        packet;
        uint16_t        seqnum;
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
};

struct pch_trdata_word_dev {
        uint32_t        word;
        pch_cuaddr_t    cuaddr;
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

struct pch_trdata_packet_sid {
        uint32_t        packet;
        uint16_t        seqnum;
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

struct pch_trdata_dma_init {
        uint32_t        ctrl;
        uint8_t         id;
        pch_dmaid_t     dmaid;
        pch_irq_index_t irq_index;
        uint8_t         core_num;
};

struct pch_trdata_chp_alloc {
        pch_sid_t       first_sid;
        uint16_t        num_devices;
        pch_chpid_t     chpid;
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
        pch_chpid_t     chpid;
        uint8_t         tx_active;
};

struct pch_trdata_cus_init_mem_channel {
        pch_cuaddr_t    cuaddr;
        pch_dmaid_t     txdmaid;
        pch_dmaid_t     rxdmaid;
};

struct pch_trdata_cus_tx_complete {
        int16_t         tx_head;
        pch_cuaddr_t    cuaddr;
        uint8_t         txpstate;
        bool            cbpending;
};

struct pch_trdata_cus_call_callback {
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
        uint8_t         cbindex;
};

struct pch_trdata_cus_register_callback {
        uint32_t        cbfunc;
        uint32_t        cbctx;
        uint8_t         cbindex;
};

struct pch_trdata_hldev_config_init {
        uint32_t        hdcfg;
        uint32_t        start;
        uint32_t        signal;
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t first_ua;
        uint8_t         num_devices;
        uint8_t         cbindex;
};

struct pch_trdata_hldev_start {
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
        uint8_t         ccwcmd;
        uint8_t         esize;
};

struct pch_trdata_hldev_data {
        uint32_t        addr;
        uint16_t        count;
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
};

struct pch_trdata_hldev_data_then {
        uint32_t        cbaddr;
        uint32_t        addr;
        uint16_t        count;
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
};

struct pch_trdata_hldev_end {
        pch_cuaddr_t    cuaddr;
        pch_unit_addr_t ua;
        uint8_t         devstat;
        uint8_t         esize;
        uint8_t         sense_flags;
        uint8_t         sense_code;
        uint8_t         sense_asc;
        uint8_t         sense_ascq;
};

struct pch_trdata_dmachan {
        pch_dmaid_t     dmaid;
};

struct pch_trdata_dmachan_memstate {
        pch_dmaid_t     dmaid;
        uint8_t         state;
};

struct pch_trdata_dmachan_segment {
        uint32_t        addr;
        uint32_t        count;
        pch_dmaid_t     dmaid;
};

struct pch_trdata_dmachan_segment_memstate {
        uint32_t        addr;
        uint32_t        count;
        pch_dmaid_t     dmaid;
        uint8_t         state;
};

struct pch_trdata_dmachan_cmd {
        uint32_t        cmd;
        uint16_t        seqnum;
        pch_dmaid_t     dmaid;
};

#endif
