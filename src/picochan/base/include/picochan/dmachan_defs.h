/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_DMACHAN_DEFS_H
#define _PCH_API_DMACHAN_DEFS_H

// dmachan_mem_src_state_t is the DMA state of a tx channel
typedef enum __attribute__((packed)) dmachan_mem_src_state {
        DMACHAN_MEM_SRC_IDLE = 0,
        DMACHAN_MEM_SRC_CMDBUF,
        DMACHAN_MEM_SRC_DATA
} dmachan_mem_src_state_t;

// dmachan_mem_dst_state_t is the DMA state of an rx channel
typedef enum __attribute__((packed)) dmachan_mem_dst_state {
        DMACHAN_MEM_DST_IDLE = 0,
        DMACHAN_MEM_DST_CMDBUF,
        DMACHAN_MEM_DST_DATA,
        DMACHAN_MEM_DST_DISCARD,
        DMACHAN_MEM_DST_SRC_ZEROES
} dmachan_mem_dst_state_t;

// dmachan_irq_reason_t represents the reason(s) why a given DMA id
// caused an interrupt for a given DMA IRQ number.
// RAISED means there was a DMA engine completion causing the bit
// for the DMA id to be set in register INTSn for that DMA IRQ index.
// FORCED means the bit for the DMA id was explicitly set in register
// INTFn for that DMA IRQ index, ignoring the value of the enable bit
// in the corresponding INTEn register.
typedef uint8_t dmachan_irq_reason_t;
#define DMACHAN_IRQ_REASON_RAISED       0x1
#define DMACHAN_IRQ_REASON_FORCED       0x2

#endif
