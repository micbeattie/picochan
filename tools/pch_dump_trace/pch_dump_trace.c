/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */

// pch_dump_trace is not intended to be compiled and run on the Pico.
// It is intended to be compiled and run on a host where picochan
// tracebuffers have been extracted and written to a file.
// Currently, I'm being lazy and assuming this program is running
// on a platform with the same endianness as Pico (little-endian).
// Would just need a bit of care to twiddle some of the bufferset
// fields and timestamps to do it properly.

// Compile with:
//   cc -I .. -Wall -o pch_dump_trace pch_dump_trace.c

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "picochan/trc_records.h"
#include "picochan/txsm_state.h"
#include "format.h"

#define MAX_NUM_BUFFERS 64

#define PCH_TRC_NUM_BUFFERS MAX_NUM_BUFFERS

#include "picochan/trc.h"

#undef PCH_TRC_RT
#define PCH_TRC_RT(rt) # rt
const char *rtnames[] = {
#include "picochan/trc_record_types.h"
};

#define NUM_RECORD_TYPES (sizeof(rtnames)/sizeof(rtnames[0]))

#define FORMATTED_TRDATA_BUFSIZE 1024

bool raw = false;

static const char *pick_side(uint rt, uint cssrt) {
        return (rt == cssrt) ? "CSS" : "CU-side";
}

static const char *pick_idtype(uint rt, uint cssrt) {
        return (rt == cssrt) ? "CHPID" : "CU";
}

static const char *pick_irqtype(uint rt) {
        switch (rt) {
        case PCH_TRC_RT_CSS_SET_FUNC_IRQ:
                return "function";

        case PCH_TRC_RT_CSS_SET_IO_IRQ:
                return "I/O";
        }

        return "unknown";
}

void hexdump(unsigned char *data, int data_size) {
        while (data_size--) {
                printf("%02x", *data++);
                if (data_size)
                        putchar(' ');
        }
}

void hexdump_trace_record_data(uint rt, unsigned char *data, int data_size) {
        const char *rtname = "USER";
        if (rt < NUM_RECORD_TYPES)
                rtname = rtnames[rt];

        printf("%s(%d) ", rtname, rt);
        hexdump(data, data_size);
}

static void print_css_sch_start(uint rt, void *vd) {
        struct pch_trdata_word_sid_byte *td = vd;
        printf("start subchannel ");
        print_sid(td->sid);
        putchar(' ');
        print_ccwaddr(td->word);
        putchar(' ');
        print_cc(td->byte);
}

static void print_css_sch_resume(uint rt, void *vd) {
        struct pch_trdata_sid_byte *td = vd;
        print_sch_func(td, "resume");
}

static void print_css_sch_cancel(uint rt, void *vd) {
        struct pch_trdata_sid_byte *td = vd;
        print_sch_func(td, "cancel");
}

static void print_css_sch_halt(uint rt, void *vd) {
        struct pch_trdata_sid_byte *td = vd;
        print_sch_func(td, "halt");
}

static void print_css_sch_test(uint rt, void *vd) {
        struct pch_trdata_scsw_sid_cc *td = vd;
        printf("test subchannel ");
        print_sid(td->sid);
        putchar(' ');
        print_cc(td->cc);
        if (td->cc == 0) {
                putchar(' ');
                print_scsw(&td->scsw);
        }
}

static void print_css_sch_store(uint rt, void *vd) {
        struct pch_trdata_sid_byte *td = vd;
        printf("store subchannel ");
        print_sid(td->sid);
        putchar(' ');
        print_cc(td->byte);
}

static void print_css_sch_modify(uint rt, void *vd) {
        struct pch_trdata_sid_byte *td = vd;
        printf("modify subchannel ");
        print_sid(td->sid);
        putchar(' ');
        print_cc(td->byte);
}

static void print_css_func_irq(uint rt, void *vd) {
        struct pch_trdata_func_irq *td = vd;
        printf("CSS Function IRQ raised for CU=%d with pending UA=%d while tx_active=%d",
                td->chpid, td->ua_opt, td->tx_active);
}

static void print_css_ccw_fetch(uint rt, void *vd) {
        struct pch_trdata_ccw_addr_sid *td = vd;
        printf("CSS CCW fetch for ");
        print_sid(td->sid);
        putchar(' ');
        print_ccwaddr(td->addr);
        printf(" provides ");
        print_ccw(td->ccw);
}

static void print_css_chp_alloc(uint rt, void *vd) {
        struct pch_trdata_chp_alloc *td = vd;
        printf("CHPID=%d allocates %d subchannels starting with ",
                td->chpid, td->num_devices);
        print_sid(td->first_sid);
}

static void print_css_chp_tx_dma_init(uint rt, void *vd) {
        struct pch_trdata_dma_init *td = vd;
        print_dma_irq_init(td, "CHPID", "tx");
}

static void print_css_chp_rx_dma_init(uint rt, void *vd) {
        struct pch_trdata_dma_init *td = vd;
        print_dma_irq_init(td, "CHPID", "rx");
}

static void print_configured(uint rt, void *vd) {
        struct pch_trdata_id_byte *td = vd;
        const char *idtype = pick_idtype(rt, PCH_TRC_RT_CSS_CHP_CONFIGURED);
        printf("%s=%d is now %s",
                idtype, td->id, td->byte ? "configured" : "unconfigured");
}

static void print_traced(uint rt, void *vd) {
        struct pch_trdata_id_byte *td = vd;
        const char *idtype = pick_idtype(rt, PCH_TRC_RT_CSS_CHP_TRACED);
        printf("%s=%d is now %s",
                idtype, td->id, td->byte ? "traced" : "untraced");
}

static void print_started(uint rt, void *vd) {
        struct pch_trdata_id_byte *td = vd;
        const char *idtype = pick_idtype(rt, PCH_TRC_RT_CSS_CHP_STARTED);
        printf("%s=%d is now %s",
                idtype, td->id, td->byte ? "started" : "stopped");
}

static void print_dma_irq(uint rt, void *vd) {
        struct pch_trdata_id_irq *td = vd;
        printf("DMA IRQ for channel %d with irq_index=%d tx:irq_state=",
               td->id, td->irq_index);
        print_dma_irq_state(td->tx_state >> 4);
        printf(",mem_src_state=");
        print_mem_src_state(td->tx_state & 0xf);
        printf(" rx:irq_state=");
        print_dma_irq_state(td->rx_state >> 4);
        printf(",mem_dst_state=");
        print_mem_dst_state(td->rx_state & 0xf);
        if (td->rx_state & 0x10)
                printf(",sets rxcomplete");
}

static void print_pio_irq(uint rt, void *vd) {
        struct pch_trdata_pio_irq *td = vd;
        printf("PIO IRQ for channel %d PIO%u SM%u complete=%d",
               td->id, td->pio_num, td->sm, td->complete);
}

static void print_init_irq_handler(uint rt, void *vd) {
        struct pch_trdata_irq_handler *td = vd;
        const char *side = pick_side(rt, PCH_TRC_RT_CSS_INIT_IRQ_HANDLER);
        printf("%s initialises IRQ %u ", side, td->irqnum);
        if (td->order_priority == -1)
                printf("exclusive");
        else
                printf("shared (priority %d)", td->order_priority);

        printf(" handler to ISR addr:%08x", td->handler);
}

static void print_cus_queue_command(uint rt, void *vd) {
        struct pch_trdata_dev_byte *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" queues tx command after tail UA=%d", td->byte);
}

static void print_cus_init_async_context(uint rt, void *vd) {
        struct pch_trdata_id_byte *td = vd;
        printf("CU-side initialised async_context with threadsafe background IRQ %u at priority %u",
                td->id, td->byte);
}

static void print_cus_cu_register(uint rt, void *vd) {
        struct pch_trdata_cu_register *td = vd;
        printf("CU=%d registers with %d devices",
                td->cuaddr, td->num_devices);
}

static void print_cus_claim_irq_index(uint rt, void *vd) {
        struct pch_trdata_id_byte *td = vd;
        printf("CU-side claims irq_index %u for core %u",
                td->id, td->byte);
}

static void print_cus_cu_set_irq_index(uint rt, void *vd) {
        struct pch_trdata_id_byte *td = vd;
        printf("CU=%d sets irq_index to %u", td->id, td->byte);
}

static void print_cus_cu_tx_dma_init(uint rt, void *vd) {
        struct pch_trdata_dma_init *td = vd;
        print_dma_irq_init(td, "CU", "tx");
}

static void print_cus_cu_rx_dma_init(uint rt, void *vd) {
        struct pch_trdata_dma_init *td = vd;
        print_dma_irq_init(td, "CU", "rx");
}

static void print_css_chp_irq_progress(uint rt, void *vd) {
        struct pch_trdata_id_byte *td = vd;
        bool rxcomplete = !!(td->byte & 0x04);
        bool txcomplete = !!(td->byte & 0x02);
        bool progress = !!(td->byte & 0x01);
        printf("IRQ progress for CHP=%d: now rxcomplete=%d txcomplete=%d progress=%d",
                td->id, rxcomplete, txcomplete, progress);
}

static void print_css_send_tx_packet(uint rt, void *vd) {
        struct pch_trdata_packet_sid *td = vd;
        printf("CSS ");
        print_sid(td->sid);
        printf(" sends ");
        print_packet(td->packet, td->seqnum, true);
}

static void print_css_tx_complete(uint rt, void *vd) {
        struct pch_trdata_id_byte *td = vd;
        printf("CHPID=%d handling tx complete while txsm is ",
                td->id);
        print_txpending_state(td->byte);
}

static void print_css_core_num(uint rt, void *vd) {
        struct pch_trdata_byte *td = vd;
        printf("CSS is running on core number %d", td->byte);
}

static void print_css_set_irq_index(uint rt, void *vd) {
        struct pch_trdata_byte *td = vd;
        printf("CSS sets irq_index to %u", td->byte);
}

static void print_css_set_irq(uint rt, void *vd) {
        struct pch_trdata_irqnum_opt *td = vd;
        const char *irqtype = pick_irqtype(rt);
        int16_t irqnum_opt = td->irqnum_opt;
        if (irqnum_opt == -1) {
                printf("CSS unsets %s IRQ number", irqtype);
        } else {
                printf("CSS sets %s IRQ number to %d",
                        irqtype, irqnum_opt);
        }
}

static void print_css_set_io_callback(uint rt, void *vd) {
        struct pch_trdata_address_change *td = vd;
        print_address_change(td, "I/O callback");
}

static void print_css_io_callback(uint rt, void *vd) {
        struct pch_trdata_intcode_scsw *td = vd;
        print_io_callback(&td->intcode, &td->scsw);
}

static void print_css_rx_command_complete(uint rt, void *vd) {
        struct pch_trdata_packet_sid *td = vd;
        printf("CSS ");
        print_sid(td->sid);
        printf(" received ");
        print_packet(td->packet, td->seqnum, false);
}

static void print_css_rx_data_complete(uint rt, void *vd) {
        struct pch_trdata_sid_byte *td = vd;
        printf("CSS rx data complete for ");
        print_sid(td->sid);
        printf(" with device status:%02x", td->byte);
}

static void print_css_notify(uint rt, void *vd) {
        struct pch_trdata_sid_byte *td = vd;
        printf("CSS Notify for ");
        print_sid(td->sid);
        printf(" with device status:%02x", td->byte);
}

static void print_cus_register_callback(uint rt, void *vd) {
        struct pch_trdata_word_byte *td = vd;
        printf("registers ");
        print_devib_callback(td->byte, td->word);
}

static void print_cus_call_callback(uint rt, void *vd) {
        struct pch_trdata_cus_call_callback *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" callback %d", td->cbindex);
}

static void print_cus_send_tx_packet(uint rt, void *vd) {
        struct pch_trdata_packet_dev *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" sends ");
        print_packet(td->packet, td->seqnum, true);
}

static void print_cus_tx_complete(uint rt, void *vd) {
        struct pch_trdata_cus_tx_complete *td = vd;
        const char *cb = td->cbpending ? "is" : "not";
        printf("CU=%d handling tx complete for tx_head UA=%d, callback %s pending, txsm is ",
                td->cuaddr, td->tx_head, cb);
        print_txpending_state(td->txpstate);
}

static void print_cus_rx_command_complete(uint rt, void *vd) {
        struct pch_trdata_packet_dev *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" received ");
        print_packet(td->packet, td->seqnum, true);
}

static void print_cus_rx_data_complete(uint rt, void *vd) {
        struct pch_trdata_dev *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" rx data complete");
}

// Values for pch_trdata_dmachan_byte for PCH_TRC_RT_DMACHAN_DST_RESET
#define DMACHAN_RESET_PROGRESSING       0
#define DMACHAN_RESET_COMPLETE          1
#define DMACHAN_RESET_BYPASSED          2
#define DMACHAN_RESET_INVALID           3

static void print_dmachan_dst_reset(uint rt, void *vd) {
        struct pch_trdata_dmachan_byte *td = vd;
        printf("rx channel DMAid=%d reset ", td->dmaid);
        switch (td->byte) {
        case DMACHAN_RESET_PROGRESSING:
                printf("progressing");
                break;

        case DMACHAN_RESET_COMPLETE:
                printf("complete");
                break;

        case DMACHAN_RESET_BYPASSED:
                printf("bypassed");
                break;

        case DMACHAN_RESET_INVALID:
                printf("invalid byte received");
                break;

        default:
                printf("unknown_trace_byte(%u)", td->byte);
                break;
        }
}

static void print_dmachan_piochan_init(uint rt, void *vd) {
        struct pch_trdata_dmachan_piochan_init *td = vd;
        printf("piochan init channel %u with PIO%u irq_index=%u tx_sm=%u rx_sm=%u tx_offset=%u rx_offset=%u tx_clock_in=%u tx_data_out=%u rx_clock_out=%u rx_data_in=%u",
                td->id, td->pio_num, td->irq_index, td->tx_sm,
                td->rx_sm, td->tx_offset, td->rx_offset,
                td->tx_clock_in, td->tx_data_out, td->rx_clock_out,
                td->rx_data_in);
}

static void print_dmachan_dst_cmdbuf_remote(uint rt, void *vd) {
        struct pch_trdata_dmachan *td = vd;
        printf("rx channel DMAid=%d sets destination to cmdbuf",
                td->dmaid);
}

static void print_dmachan_dst_cmdbuf_mem(uint rt, void *vd) {
        struct pch_trdata_dmachan_byte *td = vd;
        printf("rx memchan DMAid=%d sets destination to cmdbuf while txpeer mem_src_state=",
                td->dmaid);
        print_mem_src_state(td->byte);
        if (td->byte == DMACHAN_MEM_SRC_CMDBUF)
                printf(", sets rxcomplete and forces IRQ for tx peer");
}

static void print_dmachan_dst_data_remote(uint rt, void *vd) {
        struct pch_trdata_dmachan_segment *td = vd;
        printf("rx channel DMAid=%d sets destination to data address:%08x count=%u",
                td->dmaid, td->addr, td->count);
}

static void print_dmachan_dst_data_mem(uint rt, void *vd) {
        struct pch_trdata_dmachan_segment_memstate *td = vd;
        printf("rx memchan DMAid=%d sets destination to data address:%08x count=%u while txpeer mem_src_state=",
                td->dmaid, td->addr, td->count);
        print_mem_src_state(td->state);
}

static void print_dmachan_dst_discard_remote(uint rt, void *vd) {
        struct pch_trdata_dmachan_segment *td = vd;
        printf("rx channel DMAid=%d sets destination to discard data count=%u",
                td->dmaid, td->count);
}

static void print_dmachan_dst_discard_mem(uint rt, void *vd) {
        struct pch_trdata_dmachan_segment_memstate *td = vd;
        printf("rx memchan DMAid=%d sets destination to discard data count=%u while txpeer mem_src_state=",
                td->dmaid, td->count);
        print_mem_src_state(td->state);
        if (td->state == DMACHAN_MEM_SRC_DATA)
                printf(", sets rxcomplete and forces IRQ for tx peer");
}

static void print_dmachan_src_reset_remote(uint rt, void *vd) {
        struct pch_trdata_dmachan *td = vd;
        printf("tx channel DMAid=%d reset in progress", td->dmaid);
}

static void print_dmachan_src_cmdbuf_remote(uint rt, void *vd) {
        struct pch_trdata_dmachan *td = vd;
        printf("tx channel DMAid=%d sets source to cmdbuf",
                td->dmaid);
}

static void print_dmachan_src_cmdbuf_mem(uint rt, void *vd) {
        struct pch_trdata_dmachan_byte *td = vd;
        printf("tx memchan DMAid=%d sets source to cmdbuf while rxpeer mem_dst_state=",
                td->dmaid);
        print_mem_dst_state(td->byte);
        if (td->byte == DMACHAN_MEM_DST_CMDBUF)
                printf(", forces IRQ for rx peer");
}

static void print_dmachan_src_data_remote(uint rt, void *vd) {
        struct pch_trdata_dmachan_segment *td = vd;
        printf("tx channel DMAid=%d sets source to data address:%08x count=%u",
                td->dmaid, td->addr, td->count);
}

static void print_dmachan_src_data_mem(uint rt, void *vd) {
        struct pch_trdata_dmachan_segment_memstate *td = vd;
        printf("tx memchan DMAid=%d sets source to data address:%08x count=%u while rxpeer mem_dst_state=",
                td->dmaid, td->addr, td->count);
        print_mem_dst_state(td->state);
        if (td->state == DMACHAN_MEM_DST_DISCARD)
                printf(", forces IRQ for rx peer");
}

static void print_dmachan_force_irq(uint rt, void *vd) {
        struct pch_trdata_dmachan *td = vd;
        printf("rx memchan DMAid=%d forces IRQ for tx peer",
                td->dmaid);
}

static void print_dmachan_memchan_rx_cmd(uint rt, void *vd) {
        struct pch_trdata_dmachan_cmd *td = vd;
        printf("rx memchan DMAid=%d sync receive cmd:%08x, seqnum=%d (sets rxcomplete)",
                td->dmaid, td->cmd, td->seqnum);
}

static void print_dmachan_memchan_tx_cmd(uint rt, void *vd) {
        struct pch_trdata_dmachan_cmd *td = vd;
        printf("tx memchan DMAid=%d sync writes to peer cmd:%08x, seqnum=%d (sets txcomplete)",
                td->dmaid, td->cmd, td->seqnum);
}

static void print_enable(uint rt, void *vd) {
        char *td = vd;
        printf("trace %s", td[0] ? "enabled" : "disabled");
}

static void print_hldev_config_init(uint rt, void *vd) {
        struct pch_trdata_hldev_config_init *td = vd;
        printf("CU=%d UA_range=%d", td->cuaddr, td->first_ua);
        uint8_t n = td->num_devices;
        if (td->num_devices) {
                uint8_t last_ua = td->first_ua + n - 1;
                printf("-%d (count %d)", last_ua, n);
        } else {
                printf("(invalid num_devices=0)");
        }
        printf(" hldev configuration with hdcfg:%08x callbacks start:%08x signal:%08x used cbindex=%d",
                td->hdcfg, td->start, td->signal, td->cbindex);
}

static void print_hldev_start(uint rt, void *vd) {
        struct pch_trdata_hldev_start *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        uint8_t ccwcmd = td->ccwcmd;
        uint8_t esize = td->esize;
        bool write = pch_is_ccw_cmd_write(ccwcmd);
        const char *rwtype = write ? "Write" : "Read";

        printf(" hldev starts %s CCWcmd:%02x", rwtype, ccwcmd);
        if (write) {
                uint16_t size = pch_bsize_decode_raw_inline(esize);
                if (size)
                        printf(", %u bytes ready", size);
        } else {
                printf(", ");
                print_bsize(esize);
                printf(" bytes room");
        }
}

static void print_hldev_devib_callback(uint rt, void *vd) {
        struct pch_trdata_dev_byte *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" hldev state=");
        print_hldev_state(td->byte);
        printf(" in devib callback");
}

static void print_hldev_receiving(uint rt, void *vd) {
        struct pch_trdata_counts_dev *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" hldev received %u bytes, ", td->count1);
        if (td->count2)
                printf("requesting next %u bytes", td->count2);
        else
                printf("complete");
}

static void print_hldev_receive(uint rt, void *vd) {
        struct pch_trdata_hldev_data *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" hldev requesting to receive %u bytes to addr:%08x",
                td->count, td->addr);
}

static void print_hldev_receive_then(uint rt, void *vd) {
        struct pch_trdata_hldev_data_then *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" hldev requesting to receive %u bytes to addr:%08x",
                td->count, td->addr);
        printf(" then callback:%08x", td->cbaddr);
}

static void print_hldev_sending(uint rt, void *vd) {
        struct pch_trdata_counts_dev *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" hldev sending %u bytes to segment with room %u",
                td->count1, td->count2);
}

static void print_hldev_send(uint rt, void *vd) {
        struct pch_trdata_hldev_data *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" hldev will send %u bytes from addr:%08x",
                td->count, td->addr);
        if (rt == PCH_TRC_RT_HLDEV_SEND_FINAL)
                printf(" then end");
}

static void print_hldev_send_then(uint rt, void *vd) {
        struct pch_trdata_hldev_data_then *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" hldev will send %u bytes from addr:%08x",
                td->count, td->addr);
        printf(" then callback:%08x", td->cbaddr);
}

static void print_hldev_end(uint rt, void *vd) {
        struct pch_trdata_hldev_end *td = vd;
        print_cua_ua(td->cuaddr, td->ua);
        printf(" hldev ending with devstat:%02x", td->devstat);
        uint16_t size = pch_bsize_decode_raw_inline(td->esize);
        if (size) {
                printf(" advertising room=%u for immediate start data",
                        size);
        }
        if (td->sense_flags) {
                printf(" setting sense{flags:%02x code:%02x ASC:%02x ASCQ:%02x}",
                        td->sense_flags, td->sense_code,
                        td->sense_asc, td->sense_ascq);
        }
}

typedef void (*trace_record_print_func_t)(uint rt, void *data);

trace_record_print_func_t trace_record_printer_table[NUM_RECORD_TYPES] = {
	[PCH_TRC_RT_CSS_SCH_START] = print_css_sch_start,
	[PCH_TRC_RT_CSS_SCH_RESUME] = print_css_sch_resume,
	[PCH_TRC_RT_CSS_SCH_CANCEL] = print_css_sch_cancel,
	[PCH_TRC_RT_CSS_SCH_HALT] = print_css_sch_halt,
	[PCH_TRC_RT_CSS_SCH_TEST] = print_css_sch_test,
	[PCH_TRC_RT_CSS_SCH_STORE] = print_css_sch_store,
	[PCH_TRC_RT_CSS_SCH_MODIFY] = print_css_sch_modify,
	[PCH_TRC_RT_CSS_FUNC_IRQ] = print_css_func_irq,
	[PCH_TRC_RT_CSS_CCW_FETCH] = print_css_ccw_fetch,
	[PCH_TRC_RT_CSS_CHP_ALLOC] = print_css_chp_alloc,
	[PCH_TRC_RT_CSS_CHP_TX_DMA_INIT] = print_css_chp_tx_dma_init,
	[PCH_TRC_RT_CSS_CHP_RX_DMA_INIT] = print_css_chp_rx_dma_init,
	[PCH_TRC_RT_CSS_CHP_CONFIGURED] = print_configured,
	[PCH_TRC_RT_CUS_CU_CONFIGURED] = print_configured,
	[PCH_TRC_RT_CSS_CHP_TRACED] = print_traced,
	[PCH_TRC_RT_CUS_CU_TRACED] = print_traced,
	[PCH_TRC_RT_CSS_CHP_STARTED] = print_started,
	[PCH_TRC_RT_CUS_CU_STARTED] = print_started,
	[PCH_TRC_RT_CUS_QUEUE_COMMAND] = print_cus_queue_command,
	[PCH_TRC_RT_CUS_INIT_ASYNC_CONTEXT] = print_cus_init_async_context,
	[PCH_TRC_RT_CUS_CU_REGISTER] = print_cus_cu_register,
	[PCH_TRC_RT_CUS_CLAIM_IRQ_INDEX] = print_cus_claim_irq_index,
	[PCH_TRC_RT_CUS_CU_SET_IRQ_INDEX] = print_cus_cu_set_irq_index,
	[PCH_TRC_RT_CUS_CU_TX_DMA_INIT] = print_cus_cu_tx_dma_init,
	[PCH_TRC_RT_CUS_CU_RX_DMA_INIT] = print_cus_cu_rx_dma_init,
	[PCH_TRC_RT_CSS_CHP_IRQ_PROGRESS] = print_css_chp_irq_progress,
	[PCH_TRC_RT_CSS_SEND_TX_PACKET] = print_css_send_tx_packet,
	[PCH_TRC_RT_CSS_TX_COMPLETE] = print_css_tx_complete,
	[PCH_TRC_RT_CSS_SET_CORE_NUM] = print_css_core_num,
	[PCH_TRC_RT_CSS_SET_IRQ_INDEX] = print_css_set_irq_index,
	[PCH_TRC_RT_CSS_SET_FUNC_IRQ] = print_css_set_irq,
	[PCH_TRC_RT_CSS_SET_IO_IRQ] = print_css_set_irq,
	[PCH_TRC_RT_CSS_SET_IO_CALLBACK] = print_css_set_io_callback,
	[PCH_TRC_RT_CSS_INIT_IRQ_HANDLER] = print_init_irq_handler,
	[PCH_TRC_RT_CSS_IO_CALLBACK] = print_css_io_callback,
	[PCH_TRC_RT_CSS_RX_COMMAND_COMPLETE] = print_css_rx_command_complete,
	[PCH_TRC_RT_CSS_RX_DATA_COMPLETE] = print_css_rx_data_complete,
	[PCH_TRC_RT_CSS_NOTIFY] = print_css_notify,
	[PCH_TRC_RT_CUS_INIT_IRQ_HANDLER] = print_init_irq_handler,
	[PCH_TRC_RT_CUS_REGISTER_CALLBACK] = print_cus_register_callback,
	[PCH_TRC_RT_CUS_CALL_CALLBACK] = print_cus_call_callback,
	[PCH_TRC_RT_CUS_SEND_TX_PACKET] = print_cus_send_tx_packet,
	[PCH_TRC_RT_CUS_TX_COMPLETE] = print_cus_tx_complete,
	[PCH_TRC_RT_CUS_RX_COMMAND_COMPLETE] = print_cus_rx_command_complete,
	[PCH_TRC_RT_CUS_RX_DATA_COMPLETE] = print_cus_rx_data_complete,
	[PCH_TRC_RT_DMACHAN_DST_RESET] = print_dmachan_dst_reset,
        [PCH_TRC_RT_DMACHAN_PIOCHAN_INIT] = print_dmachan_piochan_init,
	[PCH_TRC_RT_DMACHAN_DST_CMDBUF_REMOTE] = print_dmachan_dst_cmdbuf_remote,
	[PCH_TRC_RT_DMACHAN_DST_CMDBUF_MEM] = print_dmachan_dst_cmdbuf_mem,
	[PCH_TRC_RT_DMACHAN_DST_DATA_REMOTE] = print_dmachan_dst_data_remote,
	[PCH_TRC_RT_DMACHAN_DST_DATA_MEM] = print_dmachan_dst_data_mem,
	[PCH_TRC_RT_DMACHAN_DST_DISCARD_REMOTE] = print_dmachan_dst_discard_remote,
	[PCH_TRC_RT_DMACHAN_DST_DISCARD_MEM] = print_dmachan_dst_discard_mem,
	[PCH_TRC_RT_DMACHAN_SRC_RESET_REMOTE] = print_dmachan_src_reset_remote,
	[PCH_TRC_RT_DMACHAN_SRC_CMDBUF_REMOTE] = print_dmachan_src_cmdbuf_remote,
	[PCH_TRC_RT_DMACHAN_SRC_CMDBUF_MEM] = print_dmachan_src_cmdbuf_mem,
	[PCH_TRC_RT_DMACHAN_SRC_DATA_REMOTE] = print_dmachan_src_data_remote,
	[PCH_TRC_RT_DMACHAN_SRC_DATA_MEM] = print_dmachan_src_data_mem,
	[PCH_TRC_RT_DMACHAN_FORCE_IRQ] = print_dmachan_force_irq,
	[PCH_TRC_RT_DMACHAN_MEMCHAN_RX_CMD] = print_dmachan_memchan_rx_cmd,
	[PCH_TRC_RT_DMACHAN_MEMCHAN_TX_CMD] = print_dmachan_memchan_tx_cmd,
	[PCH_TRC_RT_DMACHAN_DMA_IRQ] = print_dma_irq,
	[PCH_TRC_RT_DMACHAN_PIO_IRQ] = print_pio_irq,
	[PCH_TRC_RT_TRC_ENABLE] = print_enable,
	[PCH_TRC_RT_HLDEV_CONFIG_INIT] = print_hldev_config_init,
	[PCH_TRC_RT_HLDEV_START] = print_hldev_start,
	[PCH_TRC_RT_HLDEV_DEVIB_CALLBACK] = print_hldev_devib_callback,
	[PCH_TRC_RT_HLDEV_RECEIVING] = print_hldev_receiving,
	[PCH_TRC_RT_HLDEV_RECEIVE] = print_hldev_receive,
	[PCH_TRC_RT_HLDEV_RECEIVE_THEN] = print_hldev_receive_then,
	[PCH_TRC_RT_HLDEV_SENDING] = print_hldev_sending,
	[PCH_TRC_RT_HLDEV_SEND] = print_hldev_send,
	[PCH_TRC_RT_HLDEV_SEND_FINAL] = print_hldev_send,
	[PCH_TRC_RT_HLDEV_SEND_THEN] = print_hldev_send_then,
	[PCH_TRC_RT_HLDEV_SEND_FINAL_THEN] = print_hldev_send_then,
	[PCH_TRC_RT_HLDEV_END] = print_hldev_end,
};

void print_trace_record_data(uint rt, unsigned char *data, int data_size) {
        void *vd = data;

        if (raw) {
                hexdump_trace_record_data(rt, data, data_size);
                return;
        }

        trace_record_print_func_t f = NULL;
        if (rt < NUM_RECORD_TYPES)
                f = trace_record_printer_table[rt];

        if (f)
                f(rt, vd);
        else
                hexdump_trace_record_data(rt, data, data_size);
}

// dump_tracebs is a crude function to dump a single trace record.
// It returns the length of the header-plus-record-data or, if an
// invalid record is found, a negative value.
int dump_trace_record(unsigned char *p) {
        pch_trc_header_t *h = (pch_trc_header_t *)p;
        uint size = h->size;
        if (size < sizeof(pch_trc_header_t))
                return -1;

        if (size >= 32)
                return -2; // sanity check for currently used records

        uint64_t tus = pch_trc_timestamp_to_us(h->timestamp);
        if (tus == 0)
                return -3;

        uint64_t tsecs = tus / 1000000;
        int uuuuuu = tus % 1000000;
        int ss = tsecs % 60;
        int tmins = tsecs / 60;
        int thours = tmins / 60;
        if (thours > 24)
                return -4; // sanity check 24 hour limit for now

        int mm = tmins % 60;
        int data_size = h->size - sizeof(pch_trc_header_t);
        if (data_size < 0)
                return -5;
        if (data_size > 32)
                return -6; // sanity check 32-byte record data limit

        uint rt = h->rec_type;

        printf("%d:%02d:%02d.%06d ", thours, mm, ss, uuuuuu);
        p += sizeof(pch_trc_header_t);
        print_trace_record_data(rt, p, data_size);

        return (int)size;
}

void dump_tracebs_buffer(int bufnum, void *buf, uint32_t buflen) {
        if (!buf)
                return;

        if (buflen < sizeof(pch_trc_header_t))
                return;

        uint32_t pos = 0;
        while (pos <= buflen - sizeof(pch_trc_header_t)) {
                unsigned char *p = (unsigned char *)buf + pos;
                printf("[%d:%05d] ", bufnum, pos);
                int n = dump_trace_record(p);
                if (n < 0) {
                        printf("[err=%d]\n", n);
                        break;
                }
                pos += n;
                putchar('\n');
        }
}

// dump_tracebs is a crude function to dump a trace bufferset.
void dump_tracebs(pch_trc_bufferset_t *bs) {
        int current_buffer_num = bs->current_buffer_num;
        int n = (current_buffer_num + 1) % bs->num_buffers;
        while (n != current_buffer_num) {
                dump_tracebs_buffer(n, bs->buffers[n], bs->buffer_size);
                n = (n + 1) % bs->num_buffers;
        }

        dump_tracebs_buffer(n, bs->buffers[n],
                bs->current_buffer_pos);
}

pch_trc_bufferset_t bs;

int main(int argc, char **argv) {
        if (argc > 1) {
                if (!strcmp(argv[1], "-r")) {
                        raw = true;
                        argc--;
                        argv++;
                }
        }

        if (argc != 3) {
                fprintf(stderr, "Usage: dump_trace [-r] bufferset_file buffers_file\n");
                exit(1);
        }

        FILE *bsf = fopen(argv[1], "rb");
        size_t hdrsize = offsetof(pch_trc_bufferset_t, buffers);
        size_t nread = fread(&bs, 1, hdrsize, bsf);
        if (nread != hdrsize) {
                fprintf(stderr, "only read %zu instead of %zu bytes from bufferset file %s\n",
                        nread, sizeof(bs), argv[1]);
                exit(1);
        }

        printf("read bufferset file %s:\n", argv[1]);
        printf("  magic = 0x%08x\n", bs.magic);
        printf("  num_buffers = %d\n", bs.num_buffers);
        printf("  buffer_size = %d\n", bs.buffer_size);
        printf("  current_buffer_num = %d\n", bs.current_buffer_num);
        printf("  current_buffer_pos = %d\n", bs.current_buffer_pos);

        // Sanity checks
        if (bs.buffer_size == 0) {
                fprintf(stderr, "buffer_size is zero\n");
                exit(1);
        }
        if (bs.num_buffers == 0) {
                fprintf(stderr, "num_buffers is zero\n");
                exit(1);
        }
        if (bs.buffer_size > 1024*1024) {
                fprintf(stderr, "buffer size is unreasonably big\n");
                exit(1);
        }
        if (bs.num_buffers > MAX_NUM_BUFFERS) {
                fprintf(stderr, "number of buffers is unreasonably big\n");
                exit(1);
        }

        FILE *bf = fopen(argv[2], "rb");
        if (!bf) {
                perror(argv[2]);
                exit(1);
        }
        for (int n = 0; n < bs.num_buffers; n++) {
                unsigned char *buf = malloc(bs.buffer_size);
                if (!buf) {
                        fprintf(stderr, "malloc failed for buffer %d", n);
                        exit(1);
                }
                size_t nread = fread(buf, 1, bs.buffer_size, bf);
                if (nread != bs.buffer_size) {
                        fprintf(stderr, "only read %zu instead of %u bytes for buffer %d from file %s\n",
                                nread, bs.buffer_size, n, argv[2]);
                        exit(1);
                }
                bs.buffers[n] = buf;
                printf("read buffer %d from file %s\n", n, argv[2]);
        }

        dump_tracebs(&bs);
        exit(0);
}
