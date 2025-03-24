/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#include "pico.h"
#include "picochan/bsize.h"

uint8_t __time_critical_func(pch_bsize_encode_raw)(uint16_t n) {
        return pch_bsize_encode_raw_inline(n);
}

pch_bsizex_t __time_critical_func(pch_bsize_encodex)(uint16_t n) {
        return pch_bsize_encodex_inline(n);
}

pch_bsize_t __time_critical_func(pch_bsize_encode)(uint16_t n) {
        return pch_bsize_encode_inline(n);
}

uint16_t __time_critical_func(pch_bsize_decode_raw)(uint8_t esize) {
        return pch_bsize_decode_raw_inline(esize);
}

uint16_t __time_critical_func(pch_bsize_decode)(pch_bsize_t bsize) {
        return pch_bsize_decode_inline(bsize);
}
