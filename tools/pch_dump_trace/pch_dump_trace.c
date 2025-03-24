/*
 * Copyright (c) 2025 Malcolm Beattie
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

#define MAX_NUM_BUFFERS 64

#define PCH_TRC_NUM_BUFFERS MAX_NUM_BUFFERS

#include "picochan/trc.h"

#undef PCH_TRC_RT
#define PCH_TRC_RT(rt) # rt
const char *rtnames[] = {
#include "picochan/trc_record_types.h"
};

#define NUM_RECORD_TYPES (sizeof(rtnames)/sizeof(rtnames[0]))

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
        const char *rtname = "?";
        if (rt < NUM_RECORD_TYPES)
                rtname = rtnames[rt];

        printf("%d:%02d:%02d.%06d %s(%d) ",
                thours, mm, ss, uuuuuu, rtname, rt);
        p += sizeof(pch_trc_header_t);
        while (data_size--) {
                printf("%02x", *p++);
                if (data_size)
                        putchar(' ');
        }

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
        if (argc != 3) {
                fprintf(stderr, "Usage: dump_trace bufferset_file buffers_file\n");
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
