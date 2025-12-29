/*
 * Copyright (c) 2025 Malcolm Beattie
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "picochan/trc_records.h"
#include "picochan/txsm_state.h"
#include "format.h"

void print_sid(pch_sid_t sid) {
        printf("SID:%04x", sid);
}

void print_cc(uint8_t cc) {
        printf("cc=%d", cc);
}

void print_cua_ua(pch_cuaddr_t cua, pch_unit_addr_t ua) {
        printf("CU=%d UA=%d", cua, ua);
}

void print_address_change(struct pch_trdata_address_change *td, const char *s) {
        printf("%s address changes from %08x to %08x",
                s, td->old_addr, td->new_addr);
}

void print_ccwaddr(uint32_t ccwaddr) {
        printf("CCW address=%08x", ccwaddr);
}

void print_scsw(pch_scsw_t *scsw) {
        printf("SCSW{user_flags:%02x ctrl_flags:%02x ccw_addr:%08x devs:%02x schs:%02x count=%d}",
                scsw->user_flags, scsw->ctrl_flags, scsw->ccw_addr,
                scsw->devs, scsw->schs, scsw->count);
}

void print_io_callback(pch_intcode_t *ic, pch_scsw_t *scsw) {
        printf("CSS I/O Callback ");
        if (!ic->cc) {
                printf(" with no interrupt pending");
                return;
        }

        print_sid(ic->sid);
        printf(" ISC=%d intparm:%08x ", ic->flags, ic->intparm);
        print_scsw(scsw);
}

void print_ccw(pch_ccw_t ccw) {
        printf("CCW{cmd:%02x flags:%02x count=%d addr:%08x}",
                ccw.cmd, ccw.flags, ccw.count, ccw.addr);
}

void print_sch_func(struct pch_trdata_sid_byte *td, const char *funcname) {
        printf("%s subchannel ", funcname);
        print_sid(td->sid);
        putchar(' ');
        print_cc(td->byte);
}

void print_dma_irq_state(uint8_t state) {
        switch (state & DMACHAN_IRQ_REASON_MASK) {
        case 0:
                printf("none");
                break;
        case DMACHAN_IRQ_REASON_RAISED:
                printf("raised");
                break;
        case DMACHAN_IRQ_REASON_FORCED:
                printf("forced");
                break;
        case DMACHAN_IRQ_REASON_RAISED|DMACHAN_IRQ_REASON_FORCED:
                printf("raised+forced");
                break;
        }

        if (state & DMACHAN_IRQ_COMPLETE)
                printf("+complete");

        uint8_t badflags = state
                & ~(DMACHAN_IRQ_REASON_MASK|DMACHAN_IRQ_COMPLETE);
        if (badflags)
                printf("|unknown(%02x)", badflags);
}

void print_mem_src_state(dmachan_mem_src_state_t srcstate) {
        switch (srcstate) {
        case DMACHAN_MEM_SRC_IDLE:
                printf("idle");
                break;
        case DMACHAN_MEM_SRC_CMDBUF:
                printf("cmdbuf");
                break;
        case DMACHAN_MEM_SRC_DATA:
                printf("data");
                break;
        default:
                printf("unknown:%02x", srcstate);
                break;
        }
}

void print_mem_dst_state(dmachan_mem_dst_state_t dststate) {
        switch (dststate) {
        case DMACHAN_MEM_DST_IDLE:
                printf("idle");
                break;
        case DMACHAN_MEM_DST_CMDBUF:
                printf("cmdbuf");
                break;
        case DMACHAN_MEM_DST_DATA:
                printf("data");
                break;
        case DMACHAN_MEM_DST_DISCARD:
                printf("discard");
                break;
        case DMACHAN_MEM_DST_SRC_ZEROES:
                printf("src_zeroes");
                break;
        default:
                printf("unknown:%02x", dststate);
                break;
        }
}

void print_devib_callback(uint8_t cbindex, uint32_t cbaddr) {
        printf("devib callback %d function address:%08x",
                cbindex, cbaddr);
}

void print_dma_irq_init(struct pch_trdata_dma_init *td, const char *idtype, const char *dir) {
        printf("%s=%d initialises DMAid=%d DMA_IRQ_%d ctrl:%08x for %s from core %u",
                idtype, td->id, td->dmaid, td->irq_index, td->ctrl,
                dir, td->core_num);
}

void print_txpending_state(uint8_t txpstate) {
        switch (txpstate) {
        case PCH_TXSM_IDLE:
                printf("idle");
                break;
        case PCH_TXSM_PENDING:
                printf("pending");
                break;
        case PCH_TXSM_SENDING:
                printf("sending");
                break;
        default:
                printf("unknown(%d)", txpstate);
                break;
        }
}

void print_bsize(uint8_t esize) {
        uint16_t size = pch_bsize_decode_raw_inline(esize);
        pch_bsizex_t bx = pch_bsize_encodex_inline(size);
        const char *exactness = bx.exact ? "exact" : "inexact";
        printf("%u(%s)", size, exactness);
}

void print_packet(uint32_t raw, uint16_t seqnum, bool from_css) {
        proto_packet_t p = *(proto_packet_t*)&raw;
        proto_chop_cmd_t cmd = proto_chop_cmd(p.chop);
        proto_chop_flags_t flags = proto_chop_flags(p.chop);
        printf("packet{");
        switch (cmd) {
        case PROTO_CHOP_START: {
                printf("Start");
                if (from_css) {
                        if (flags & PROTO_CHOP_FLAG_SKIP)
                                printf("|Skip");
                        flags &= ~PROTO_CHOP_FLAG_SKIP;
                }
                if (flags)
                        printf("|UnknownFlags:%02x", flags);

                printf(" ua=%d CCWcmd:%02x count=",
                        p.unit_addr, p.p0);
                print_bsize(p.p1);
                break;
        }
        case PROTO_CHOP_ROOM:
                printf("Room");
                if (from_css) {
                        if (flags & PROTO_CHOP_FLAG_SKIP)
                                printf("|Skip");
                        flags &= ~PROTO_CHOP_FLAG_SKIP;
                }
                if (flags)
                        printf("|UnknownFlags:%02x", flags);
                printf(" ua=%d count=%u", p.unit_addr,
                        proto_get_count(p));
                break;
        case PROTO_CHOP_DATA:
                printf("Data");
                if (flags & PROTO_CHOP_FLAG_SKIP)
                        printf("|Skip");
                flags &= ~PROTO_CHOP_FLAG_SKIP;
                if (flags & PROTO_CHOP_FLAG_END)
                        printf("|End");
                flags &= ~PROTO_CHOP_FLAG_END;
                if (from_css) {
                        if (flags & PROTO_CHOP_FLAG_STOP)
                                printf("|Stop");
                        flags &= ~PROTO_CHOP_FLAG_STOP;
                } else {
                        if (flags & PROTO_CHOP_FLAG_RESPONSE_REQUIRED)
                                printf("|ResponseRequired");
                        flags &= ~PROTO_CHOP_FLAG_RESPONSE_REQUIRED;
                }
                if (flags)
                        printf("|UnknownFlags:%02x", flags);
                printf(" ua=%d count=%u", p.unit_addr,
                        proto_get_count(p));
                break;
        case PROTO_CHOP_UPDATE_STATUS:
                printf("UpdateStatus ua=%d devs:%02x advertise=",
                        p.unit_addr, p.p0);
                print_bsize(p.p1);
                break;
        case PROTO_CHOP_REQUEST_READ:
                printf("RequestRead ua=%d count=%u", p.unit_addr,
                        proto_get_count(p));
                break;
        case PROTO_CHOP_HALT:
                printf("Halt ua=%d", p.unit_addr);
                break;
        default:
                printf("Unknown(chop_cmd=%d flags:%02x ua=%d p0:%02x p1:%02x)",
                        cmd, flags, p.unit_addr, p.p0, p.p1);
                break;
        }
        if (seqnum)
                printf(" seqnum=%d", seqnum);

        printf("}");
}

// Values for pch_hldev_t state field from picochan/hldev.h
#define PCH_HLDEV_IDLE          0
#define PCH_HLDEV_STARTED       1
#define PCH_HLDEV_RECEIVING     2
#define PCH_HLDEV_SENDING       3
#define PCH_HLDEV_SENDING_FINAL 4
#define PCH_HLDEV_ENDING        5

const char *hldev_state[] = {
	[PCH_HLDEV_IDLE] = "idle",
	[PCH_HLDEV_STARTED] = "started",
	[PCH_HLDEV_RECEIVING] = "receiving",
	[PCH_HLDEV_SENDING] = "sending",
	[PCH_HLDEV_SENDING_FINAL] = "sending_final",
	[PCH_HLDEV_ENDING] = "ending"
};

void print_hldev_state(uint8_t state) {
       if (state >= sizeof(hldev_state)/sizeof(hldev_state[0]))
                printf("?(%u)", state);
        else
                printf("%s(%u)", hldev_state[state], state);
}
