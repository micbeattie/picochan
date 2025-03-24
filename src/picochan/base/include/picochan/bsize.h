/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_BSIZE_H
#define _PCH_API_BSIZE_H

#include <stdint.h>

// pch_bsize_t represents a uint16_t, n, encoded into a
// (type-wrapped 8-bit) value that decodes to a uint16_t that is less
// than or equal to n but "close" when n is a size typically used as a
// buffer size for workloads using Picochan.
// The encoding/decoding is exact for the following values:
//  1 x [0,63] -> 0,1,2,...,63
//  2 x [32,95] -> 64,66,68,...,190
//  8 x [24,87] -> 192,200,208,...,696
//  64 x [11,74] -> 704,768,832,...,4736
typedef struct pch_bsize {
        uint8_t esize;
} pch_bsize_t;

#define PCH_BSIZE_ZERO ((pch_bsize_t){0})

// pch_bsizex_t contains a pch_bsize_t along with a flag to indicate
// whether the bsize encoded the original size exactly. The flag
// is the low bit of pch_bsizex_t.exact.
typedef struct pch_bsizex {
        uint8_t         exact;
        pch_bsize_t     bsize;
} pch_bsizex_t;

// Non-inlined API functions
pch_bsizex_t pch_bsize_encodex(uint16_t n);

pch_bsize_t pch_bsize_encode(uint16_t n);

uint16_t pch_bsize_decode_raw(uint8_t esize);

uint16_t pch_bsize_decode(pch_bsize_t bsize);

// Inline encode/decode operations

// pch_bsize_unwrap unwraps the uint8_t used to encode the size.
static inline uint8_t pch_bsize_unwrap(pch_bsize_t s) {
        return s.esize;
}

// pch_bsize_t wraps a uint8, typically obtained from receiving an
// unwrapped bsize over a remote protocol to produce its size.
static inline pch_bsize_t pch_bsize_wrap(uint8_t esize) {
        return (pch_bsize_t){esize};
}

// pch_bsize_encode_raw_inline encodes size as its pch_bsize_t
// encoding. It is a shortcut for
// pch_bsize_unwrap(pch_bsize_encode(size)) which can be used when the
// benefits of the type-wrapping of the encoding are not needed.
static inline uint8_t pch_bsize_encode_raw_inline(uint16_t n) {
	// XXX TODO See if we can just call pch_bsize_encodex_inline
	// and return the contained pch_bsize_t and have gcc
	// reliably optimise it as well as not calculating the
	// exact flag in the first place. For now we just spell it
	// all out again.

	// 0b00nnnnnn - 1 x [0,63] -> 0,1,2,...,63
	if (n <= 63)
                return (uint8_t)n;

	// 0b01nnnnnn - 2 x [32,95] -> 64,66,68,...,190
	if (n <= 191)
                return (uint8_t)(((n>>1) - 32) | 0x40);

	// 0b10nnnnnn - 8 x [24,87] -> 192,200,208,...,696
	if (n <= 703)
                return (uint8_t)(((n>>3) - 24) | 0x80);

	// 0b11nnnnnn - 64 x [11,74] -> 704,768,832,...,4736
	if (n <= 4736)
                return (uint8_t)(((n>>6) - 11) | 0xc0);

        return 0xff;
}

// pch_bsize_encodex_inline encodes n into its pch_bsize_t along
// with a flag bit that indicates whether decoding the result will
// produce exactly n.
// This function is declared as "static inline" to be used in places
// where it is appropriate to have the code inlined. A corresponding
// function pch_bsize_encodex is available as an ordinary function.
static inline pch_bsizex_t pch_bsize_encodex_inline(uint16_t n) {
	// 0b00nnnnnn - 1 x [0,63] -> 0,1,2,...,63
	if (n <= 63)
                return (pch_bsizex_t){1,pch_bsize_wrap(n)};

	// 0b01nnnnnn - 2 x [32,95] -> 64,66,68,...,190
	if (n <= 191) {
		uint8_t exact = (n & 0x1) == 0;
                pch_bsize_t bsize = pch_bsize_wrap(((n>>1) - 32) | 0x40);
		return (pch_bsizex_t){exact,bsize};
	}

	// 0b10nnnnnn - 8 x [24,87] -> 192,200,208,...,696
	if (n <= 703) {
		uint8_t exact = (n & 0x7) == 0;
                pch_bsize_t bsize = pch_bsize_wrap(((n>>3) - 24) | 0x80);
		return (pch_bsizex_t){exact,bsize};
	}

	// 0b11nnnnnn - 64 x [11,74] -> 704,768,832,...,4736
	if (n <= 4736) {
		uint8_t exact = (n & 0x3f) == 0;
                pch_bsize_t bsize = pch_bsize_wrap(((n>>6) - 11) | 0xc0);
                return (pch_bsizex_t){exact,bsize};
	}

        return (pch_bsizex_t){0, pch_bsize_wrap(0xff)};
}

// pch_bsize_encode_inline does the same as pch_bsize_encodex_inline
// but does not return the exactness.
static inline pch_bsize_t pch_bsize_encode_inline(uint16_t n) {
        return pch_bsize_wrap(pch_bsize_encode_raw_inline(n));
}

// pch_bsize_decode_raw decodes encoded size esize. It is a shortcut
// for pch_bsize_decode(pch_bsize_wrap(esize) which can be used when
// the benefits of the type-wrapping of the encoding are not needed.
static inline uint16_t pch_bsize_decode_raw_inline(uint8_t esize) {
        uint8_t flags = esize & 0xc0;
        uint16_t n = esize & 0x3f;

	switch (flags) {
	case 0x00:
		// 0b00nnnnnn - 1 x [0,63] -> 0,1,2,...,63
		return n;

	case 0x40:
		// 0b01nnnnnn - 2 x [32,95] -> 64,66,68,...,190
		return (n+32) << 1;

	case 0x80:
		// 0b10nnnnnn - 8 x [24,87] -> 192,200,208,...,696
		return (n+24) << 3;
	}

	// 0b11nnnnnn - 64 x [11,74] -> 704,768,832,...,4736
	return (n+11) << 6;
}

// pch_bsize_decode_inline decodes a pch_bsize_t as the uint16_t it
// represents.
// This function is declared as "static inline" to be used in places
// where it is appropriate to have the code inlined. A corresponding
// function pch_bsize_decode is available as an ordinary function.
static inline uint16_t pch_bsize_decode_inline(pch_bsize_t bsize) {
        return pch_bsize_decode_raw_inline(bsize.esize);
}

uint8_t pch_bsize_encode_raw(uint16_t n);

#endif
