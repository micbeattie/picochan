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

static const char *pick_idtype(uint rt, uint cssrt) {
        return (rt == cssrt) ? "CHPID" : "CU";
}

static const char *pick_irqtype(uint rt) {
        switch (rt) {
        case PCH_TRC_RT_CSS_INIT_DMA_IRQ_HANDLER:
        case PCH_TRC_RT_CSS_SET_DMA_IRQ:
                return "DMA";

        case PCH_TRC_RT_CSS_INIT_FUNC_IRQ_HANDLER:
        case PCH_TRC_RT_CSS_SET_FUNC_IRQ:
                return "function";

        case PCH_TRC_RT_CSS_INIT_IO_IRQ_HANDLER:
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

void print_trace_record_data(uint rt, unsigned char *data, int data_size) {
        void *vd = data;

        if (raw) {
                hexdump_trace_record_data(rt, data, data_size);
                return;
        }

        switch (rt) {
        case PCH_TRC_RT_CSS_SCH_START: {
                struct pch_trdata_word_sid_byte *td = vd;
                printf("start subchannel ");
                print_sid(td->sid);
                putchar(' ');
                print_ccwaddr(td->word);
                putchar(' ');
                print_cc(td->byte);
                break;
        }

        case PCH_TRC_RT_CSS_SCH_RESUME: {
                struct pch_trdata_sid_byte *td = vd;
                print_sch_func(td, "resume");
                break;
        }

        case PCH_TRC_RT_CSS_SCH_CANCEL: {
                struct pch_trdata_sid_byte *td = vd;
                print_sch_func(td, "cancel");
                break;
        }

        case PCH_TRC_RT_CSS_SCH_HALT: {
                struct pch_trdata_sid_byte *td = vd;
                print_sch_func(td, "halt");
                break;
        }

        case PCH_TRC_RT_CSS_SCH_TEST: {
                struct pch_trdata_scsw_sid_cc *td = vd;
                printf("test subchannel ");
                print_sid(td->sid);
                putchar(' ');
                print_cc(td->cc);
                if (td->cc == 0) {
                        putchar(' ');
                        print_scsw(&td->scsw);
                }
                break;
        }

        case PCH_TRC_RT_CSS_SCH_STORE: {
                struct pch_trdata_sid_byte *td = vd;
                printf("store subchannel ");
                print_sid(td->sid);
                putchar(' ');
                print_cc(td->byte);
                break;
        }

        case PCH_TRC_RT_CSS_SCH_MODIFY: {
                struct pch_trdata_sid_byte *td = vd;
                printf("modify subchannel ");
                print_sid(td->sid);
                putchar(' ');
                print_cc(td->byte);
                break;
        }

        case PCH_TRC_RT_CSS_FUNC_IRQ: {
                struct pch_trdata_func_irq *td = vd;
                printf("CSS Function IRQ raised for CU=%d with pending UA=%d while tx_active=%d",
                        td->chpid, td->ua_opt, td->tx_active);
                break;
        }

        case PCH_TRC_RT_CSS_CCW_FETCH: {
                struct pch_trdata_ccw_addr_sid *td = vd;
                printf("CSS CCW fetch for ");
                print_sid(td->sid);
                putchar(' ');
                print_ccwaddr(td->addr);
                printf(" provides ");
                print_ccw(td->ccw);
                break;
        }

        case PCH_TRC_RT_CSS_CHP_ALLOC: {
                struct pch_trdata_chp_alloc *td = vd;
                printf("CHPID=%d allocates %d subchannels starting with ",
                        td->chpid, td->num_devices);
                print_sid(td->first_sid);
                break;
        }

        case PCH_TRC_RT_CSS_CHP_TX_DMA_INIT: {
                struct pch_trdata_dma_init *td = vd;
                print_dma_irq_init(td, "CHPID", "tx");
                break;
        }

        case PCH_TRC_RT_CSS_CHP_RX_DMA_INIT: {
                struct pch_trdata_dma_init *td = vd;
                print_dma_irq_init(td, "CHPID", "rx");
                break;
        }

        case PCH_TRC_RT_CSS_INIT_DMA_IRQ_HANDLER:
        case PCH_TRC_RT_CSS_INIT_FUNC_IRQ_HANDLER:
        case PCH_TRC_RT_CSS_INIT_IO_IRQ_HANDLER: {
                const char *irqtype = pick_irqtype(rt);
                struct pch_trdata_irq_handler *td = vd;
                printf("CSS ");
                print_dma_handler_init(td, irqtype);
                break;
        }

        case PCH_TRC_RT_CUS_QUEUE_COMMAND: {
                struct pch_trdata_dev_byte *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" queues tx command after tail UA=%d", td->byte);
                break;
        }

        case PCH_TRC_RT_CUS_INIT_DMA_IRQ_HANDLER: {
                struct pch_trdata_irq_handler *td = vd;
                printf("CU-side ");
                print_dma_handler_init(td, "DMA");
                break;
        }

        case PCH_TRC_RT_CUS_CU_REGISTER: {
                struct pch_trdata_cu_register *td = vd;
                printf("CU=%d registers with %d devices",
                        td->cuaddr, td->num_devices);
                break;
        }

        case PCH_TRC_RT_CUS_CU_TX_DMA_INIT: {
                struct pch_trdata_dma_init *td = vd;
                print_dma_irq_init(td, "CU", "tx");
                break;
        }

        case PCH_TRC_RT_CUS_CU_RX_DMA_INIT: {
                struct pch_trdata_dma_init *td = vd;
                print_dma_irq_init(td, "CU", "rx");
                break;
        }

        case PCH_TRC_RT_CSS_CHP_CONFIGURED:
        case PCH_TRC_RT_CUS_CU_CONFIGURED: {
                const char *idtype = pick_idtype(rt, PCH_TRC_RT_CSS_CHP_CONFIGURED);
                struct pch_trdata_id_byte *td = vd;
                printf("%s=%d is now %s",
                        idtype, td->id, td->byte ? "configured" : "unconfigured");
                break;
        }

        case PCH_TRC_RT_CSS_CHP_TRACED:
        case PCH_TRC_RT_CUS_CU_TRACED: {
                const char *idtype = pick_idtype(rt, PCH_TRC_RT_CSS_CHP_TRACED);
                struct pch_trdata_id_byte *td = vd;
                printf("%s=%d is now %s",
                        idtype, td->id, td->byte ? "traced" : "untraced");
                break;
        }

        case PCH_TRC_RT_CSS_CHP_STARTED:
        case PCH_TRC_RT_CUS_CU_STARTED: {
                const char *idtype = pick_idtype(rt, PCH_TRC_RT_CSS_CHP_STARTED);
                struct pch_trdata_id_byte *td = vd;
                printf("%s=%d is now %s",
                        idtype, td->id, td->byte ? "started" : "stopped");
                break;
        }

        case PCH_TRC_RT_CSS_CHP_IRQ:
        case PCH_TRC_RT_CUS_CU_IRQ: {
                const char *idtype = pick_idtype(rt, PCH_TRC_RT_CSS_CHP_IRQ);
                struct pch_trdata_id_irq *td = vd;
                printf("IRQ for %s=%d with DMA_IRQ_%d tx:irq_state=",
                       idtype, td->id, td->dmairqix);
                print_dma_irq_state(td->tx_state >> 4);
                printf(",mem_src_state=");
                print_mem_src_state(td->tx_state & 0xf);
                printf(" rx:irq_state=");
                print_dma_irq_state(td->rx_state >> 4);
                printf(",mem_dst_state=");
                print_mem_dst_state(td->rx_state & 0xf);
                if (td->rx_state & 0x10)
                        printf(",sets rxcomplete");
                break;
        }

        case PCH_TRC_RT_CSS_CHP_IRQ_PROGRESS: {
                struct pch_trdata_id_byte *td = vd;
                bool rxcomplete = !!(td->byte & 0x04);
                bool txcomplete = !!(td->byte & 0x02);
                bool progress = !!(td->byte & 0x01);
                printf("IRQ progress for CHP=%d: now rxcomplete=%d txcomplete=%d progress=%d",
                        td->id, rxcomplete, txcomplete, progress);
                break;
        }

        case PCH_TRC_RT_CSS_SEND_TX_PACKET: {
                struct pch_trdata_packet_sid *td = vd;
                printf("CSS ");
                print_sid(td->sid);
                printf(" sends ");
                print_packet(td->packet, td->seqnum, true);
                break;
        }

        case PCH_TRC_RT_CSS_TX_COMPLETE: {
                struct pch_trdata_id_byte *td = vd;
                printf("CHPID=%d handling tx complete while txsm is ",
                        td->id);
                print_txpending_state(td->byte);
                break;
        }

        case PCH_TRC_RT_CSS_CORE_NUM: {
                struct pch_trdata_byte *td = vd;
                printf("CSS is running on core number %d", td->byte);
                break;
        }

        case PCH_TRC_RT_CSS_SET_DMA_IRQ:
        case PCH_TRC_RT_CSS_SET_FUNC_IRQ:
        case PCH_TRC_RT_CSS_SET_IO_IRQ: {
                const char *irqtype = pick_irqtype(rt);
                struct pch_trdata_irqnum_opt *td = vd;
                int16_t irqnum_opt = td->irqnum_opt;
                if (irqnum_opt == -1) {
                        printf("CSS unsets %s IRQ number", irqtype);
                } else {
                        printf("CSS sets %s IRQ number to %d",
                                irqtype, irqnum_opt);
                }

                break;
        }

        case PCH_TRC_RT_CSS_SET_IO_CALLBACK: {
                struct pch_trdata_address_change *td = vd;
                print_address_change(td, "I/O callback");
                break;
        }

        case PCH_TRC_RT_CSS_IO_CALLBACK: {
                struct pch_trdata_intcode_scsw *td = vd;
                print_io_callback(&td->intcode, &td->scsw);
                break;
        }

        case PCH_TRC_RT_CSS_RX_COMMAND_COMPLETE: {
                struct pch_trdata_packet_sid *td = vd;
                printf("CSS ");
                print_sid(td->sid);
                printf(" received ");
                print_packet(td->packet, td->seqnum, false);
                break;
        }

        case PCH_TRC_RT_CSS_RX_DATA_COMPLETE: {
                struct pch_trdata_sid_byte *td = vd;
                printf("CSS rx data complete for ");
                print_sid(td->sid);
                printf(" with device status:%02x", td->byte);
                break;
        }

        case PCH_TRC_RT_CSS_NOTIFY: {
                struct pch_trdata_sid_byte *td = vd;
                printf("CSS Notify for ");
                print_sid(td->sid);
                printf(" with device status:%02x", td->byte);
                break;
        }

        case PCH_TRC_RT_CUS_REGISTER_CALLBACK: {
                struct pch_trdata_word_byte *td = vd;
                printf("registers ");
                print_devib_callback(td->byte, td->word);
                break;
        }

        case PCH_TRC_RT_CUS_CALL_CALLBACK: {
                struct pch_trdata_cus_call_callback *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" callback %d from %u", td->cbindex, td->from);
                break;
        }

        case PCH_TRC_RT_CUS_SEND_TX_PACKET: {
                struct pch_trdata_packet_dev *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" sends ");
                print_packet(td->packet, td->seqnum, true);
                break;
        }

        case PCH_TRC_RT_CUS_TX_COMPLETE: {
                struct pch_trdata_cus_tx_complete *td = vd;
                const char *cb = td->tx_callback ? "set" : "unset";
                printf("CU=%d handling tx complete for tx_head UA=%d, tx_callback %s, txsm is ",
                        td->cuaddr, td->tx_head, cb);
                print_txpending_state(td->txpstate);
                break;
        }

        case PCH_TRC_RT_CUS_RX_COMMAND_COMPLETE: {
                struct pch_trdata_packet_dev *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" received ");
                print_packet(td->packet, td->seqnum, true);
                break;
        }

        case PCH_TRC_RT_CUS_RX_DATA_COMPLETE: {
                struct pch_trdata_dev *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" rx data complete");
                break;
        }

	case PCH_TRC_RT_DMACHAN_DST_RESET_REMOTE: {
		struct pch_trdata_dmachan *td = vd;
                printf("rx channel DMAid=%d reset in progress",
                        td->dmaid);
                break;
	}

	case PCH_TRC_RT_DMACHAN_DST_CMDBUF_REMOTE: {
		struct pch_trdata_dmachan *td = vd;
                printf("rx channel DMAid=%d sets destination to cmdbuf",
                        td->dmaid);
                break;
	}

	case PCH_TRC_RT_DMACHAN_DST_CMDBUF_MEM: {
		struct pch_trdata_dmachan_memstate *td = vd;
                printf("rx memchan DMAid=%d sets destination to cmdbuf while txpeer mem_src_state=",
                        td->dmaid);
                print_mem_src_state(td->state);
                if (td->state == DMACHAN_MEM_SRC_CMDBUF)
                        printf(", sets rxcomplete and forces IRQ for tx peer");
                break;
	}

	case PCH_TRC_RT_DMACHAN_DST_DATA_REMOTE: {
		struct pch_trdata_dmachan_segment *td = vd;
                printf("rx channel DMAid=%d sets destination to data address:%08x count=%u",
                        td->dmaid, td->addr, td->count);
                break;
	}

	case PCH_TRC_RT_DMACHAN_DST_DATA_MEM: {
		struct pch_trdata_dmachan_segment_memstate *td = vd;
                printf("rx memchan DMAid=%d sets destination to data address:%08x count=%u while txpeer mem_src_state=",
                        td->dmaid, td->addr, td->count);
                print_mem_src_state(td->state);
                break;
	}

	case PCH_TRC_RT_DMACHAN_DST_DISCARD_REMOTE: {
		struct pch_trdata_dmachan_segment *td = vd;
                printf("rx channel DMAid=%d sets destination to discard data count=%u",
                        td->dmaid, td->count);
                break;
	}

	case PCH_TRC_RT_DMACHAN_DST_DISCARD_MEM: {
		struct pch_trdata_dmachan_segment_memstate *td = vd;
                printf("rx memchan DMAid=%d sets destination to discard data count=%u while txpeer mem_src_state=",
                        td->dmaid, td->count);
                print_mem_src_state(td->state);
                if (td->state == DMACHAN_MEM_SRC_DATA)
                        printf(", sets rxcomplete and forces IRQ for tx peer");
                break;
	}

	case PCH_TRC_RT_DMACHAN_SRC_RESET_REMOTE: {
		struct pch_trdata_dmachan *td = vd;
                printf("tx channel DMAid=%d reset in progress",
                        td->dmaid);
                break;
	}

	case PCH_TRC_RT_DMACHAN_SRC_CMDBUF_REMOTE: {
		struct pch_trdata_dmachan *td = vd;
                printf("tx channel DMAid=%d sets source to cmdbuf",
                        td->dmaid);
                break;
	}

	case PCH_TRC_RT_DMACHAN_SRC_CMDBUF_MEM: {
		struct pch_trdata_dmachan_memstate *td = vd;
                printf("tx memchan DMAid=%d sets source to cmdbuf while rxpeer mem_dst_state=",
                        td->dmaid);
                print_mem_dst_state(td->state);
                if (td->state == DMACHAN_MEM_DST_CMDBUF)
                        printf(", forces IRQ for rx peer");
                break;
	}

	case PCH_TRC_RT_DMACHAN_SRC_DATA_REMOTE: {
		struct pch_trdata_dmachan_segment *td = vd;
                printf("tx channel DMAid=%d sets source to data address:%08x count=%u",
                        td->dmaid, td->addr, td->count);
                break;
	}

	case PCH_TRC_RT_DMACHAN_SRC_DATA_MEM: {
		struct pch_trdata_dmachan_segment_memstate *td = vd;
                printf("tx memchan DMAid=%d sets source to data address:%08x count=%u while rxpeer mem_dst_state=",
                        td->dmaid, td->addr, td->count);
                print_mem_dst_state(td->state);
                if (td->state == DMACHAN_MEM_DST_DISCARD)
                        printf(", forces IRQ for rx peer");
                break;
	}

        case PCH_TRC_RT_DMACHAN_FORCE_IRQ: {
                struct pch_trdata_dmachan *td = vd;
                printf("rx memchan DMAid=%d forces IRQ for tx peer",
                        td->dmaid);
                break;
        }

        case PCH_TRC_RT_DMACHAN_MEMCHAN_RX_CMD: {
                struct pch_trdata_dmachan_cmd *td = vd;
                printf("rx memchan DMAid=%d sync receive cmd:%08x, seqnum=%d (sets rxcomplete)",
                        td->dmaid, td->cmd, td->seqnum);
                break;
        }

        case PCH_TRC_RT_DMACHAN_MEMCHAN_TX_CMD: {
                struct pch_trdata_dmachan_cmd *td = vd;
                printf("tx memchan DMAid=%d sync writes to peer cmd:%08x, seqnum=%d (sets txcomplete)",
                        td->dmaid, td->cmd, td->seqnum);
                break;
        }

        case PCH_TRC_RT_TRC_ENABLE:
                printf("trace %s", data[0] ? "enabled" : "disabled");
                break;

        case PCH_TRC_RT_HLDEV_CONFIG_INIT: {
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
                break;
        }

        case PCH_TRC_RT_HLDEV_START: {
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
                break;
        }

        case PCH_TRC_RT_HLDEV_DEVIB_CALLBACK: {
                struct pch_trdata_dev_byte *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" hldev state=");
                print_hldev_state(td->byte);
                printf(" in devib callback");
                break;
        }

        case PCH_TRC_RT_HLDEV_RECEIVING: {
                struct pch_trdata_counts_dev *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" hldev received %u bytes, ", td->count1);
                if (td->count2)
                        printf("requesting next %u bytes", td->count2);
                else
                        printf("complete");

                break;
        }

        case PCH_TRC_RT_HLDEV_RECEIVE: {
                struct pch_trdata_hldev_data *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" hldev requesting to receive %u bytes to addr:%08x",
                        td->count, td->addr);
                break;
        }

        case PCH_TRC_RT_HLDEV_RECEIVE_THEN: {
                struct pch_trdata_hldev_data_then *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" hldev requesting to receive %u bytes to addr:%08x",
                        td->count, td->addr);
                printf(" then callback:%08x", td->cbaddr);
                break;
        }

        case PCH_TRC_RT_HLDEV_SENDING: {
                struct pch_trdata_counts_dev *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" hldev sending %u bytes to segment with room %u",
                        td->count1, td->count2);
                break;
        }

        case PCH_TRC_RT_HLDEV_SEND:
        case PCH_TRC_RT_HLDEV_SEND_FINAL: {
                struct pch_trdata_hldev_data *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" hldev will send %u bytes from addr:%08x",
                        td->count, td->addr);
                if (rt == PCH_TRC_RT_HLDEV_SEND_FINAL)
                        printf(" then end");
                break;
        }

        case PCH_TRC_RT_HLDEV_SEND_THEN:
        case PCH_TRC_RT_HLDEV_SEND_FINAL_THEN: {
                struct pch_trdata_hldev_data_then *td = vd;
                print_cua_ua(td->cuaddr, td->ua);
                printf(" hldev will send %u bytes from addr:%08x",
                        td->count, td->addr);
                printf(" then callback:%08x", td->cbaddr);
                break;
        }

        case PCH_TRC_RT_HLDEV_END: {
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

                break;
        }

        default:
                hexdump_trace_record_data(rt, data, data_size);
        }
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
