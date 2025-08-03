#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "picochan/ids.h"
#include "picochan/scsw.h"
#include "picochan/ccw.h"
#include "picochan/dmachan_defs.h"
#include "picochan/trc_records.h"
#include "packet.h"

void print_sid(pch_sid_t sid);
void print_cc(uint8_t cc);
void print_address_change(struct pch_trdata_address_change *td, const char *s);
void print_ccwaddr(uint32_t ccwaddr);
void print_ccw(pch_ccw_t ccw);
void print_mem_src_state(dmachan_mem_src_state_t srcstate);
void print_mem_dst_state(dmachan_mem_dst_state_t dststate);
void print_dma_irq_state(uint8_t state);
void print_devib_callback(uint8_t cbindex, uint32_t cbaddr);
void print_dma_irq_init(struct pch_trdata_dma_init *td);
void print_txpending_state(uint8_t txpstate);
void print_chop(proto_chop_t chop);
void print_packet(uint32_t raw, bool from_css);
