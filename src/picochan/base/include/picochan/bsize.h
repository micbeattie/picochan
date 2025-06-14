/*
 * Copyright (c) 2025 Malcolm Beattie
 */

#ifndef _PCH_API_BSIZE_H
#define _PCH_API_BSIZE_H

#include <stdint.h>

/*! \file picochan/bsize.h
 *  \ingroup picochan_base
 *
 * \brief An encoding of 16-bit counts as 8-bit values for typical Pico-sized buffers
 */

/*! \brief an 8-bit structure whose value encodes a 16-bit value for use as a count of bytes in a typical picochan buffer or transfer request
 *
 * The 8-bit encoding is wrapped as a structure to provide type
 * clarity (even if not full type safety is not possible) when
 * being passed around via the API and stored.
 *
 * The encoding is not 1-1 (of course) but the decoding of the value
 * obtained by encoding n is always less than or equal to n and
 * "close" when n is a size typically used as a buffer size for
 * workloads using picochan.
 *
 * The encoding/decoding is exact for the following values:
 *
 *  * 1 x [0, 63] -> 0, 1, 2, ..., 63
 *  * 2 x [32, 95] -> 64, 66, 68, ..., 190
 *  * 8 x [24, 87] -> 192, 200, 208, ..., 696
 *  * 64 x [11, 74] -> 704, 768, 832, ..., 4736
 */
typedef struct pch_bsize {
        uint8_t esize;
} pch_bsize_t;

/*! \brief A constant struct initialiser for the bsize encoding of zero
 *  \ingroup picochan_base
 *
 * This is simply a constant structure initialiser (the structure
 * itself, not a pointer to a structure) containing a single byte
 * of zero which is the bsize encoding of zero.
 */
#define PCH_BSIZE_ZERO ((pch_bsize_t){0})

/*! \brief a pch_bsize together with a flag intended to indicate whether the bsize encoded the original size exactly.
 *  \ingroup picochan_base
 *
 * The flag is the low bit of the exact field. It is defined as
 * a uint8_t rather than a bool to make its position clearer in
 * any stored value of the structure.
 */
typedef struct pch_bsizex {
        uint8_t         exact;
        pch_bsize_t     bsize;
} pch_bsizex_t;

// Non-inlined API functions

/*! \brief Encode 16-bit count as an pch_bsizex_t
 *  \ingroup picochan_base
 */
pch_bsizex_t pch_bsize_encodex(uint16_t n);

/*! \brief Encode 16-bit count as an 8-bit pch_bsize_t
 *  \ingroup picochan_base
 */
pch_bsize_t pch_bsize_encode(uint16_t n);

/*! \brief Decode an 8-bit raw value of a bsize (not in its
 * pch_bsize_t type-wrapping) into a 16-bit value
 *  \ingroup picochan_base
 */
uint16_t pch_bsize_decode_raw(uint8_t esize);

/*! \brief Decode an 8-bit pch_bsize_t value into a
 * 16-bit value
 *  \ingroup picochan_base
 */
uint16_t pch_bsize_decode(pch_bsize_t bsize);

// Inline encode/decode operations

/*! \brief Unwraps the uint8_t contained in a pch_bsize_t
 *  \ingroup picochan_base
 */
static inline uint8_t pch_bsize_unwrap(pch_bsize_t s) {
        return s.esize;
}

/*! \brief wraps a uint8_t into a pch_bsize_t
 *  \ingroup picochan_base
 *
 * This is typically used to produce a clearly-typed
 * "bsize encoded" value after receiving an unwrapped bsize
 * from a remote protocol
 *  \ingroup picochan_base
 */
static inline pch_bsize_t pch_bsize_wrap(uint8_t esize) {
        return (pch_bsize_t){esize};
}

/*! \brief Perform a bsize encoding, returning the encoded value unwrapped
 *  \ingroup picochan_base
 *
 * This is a shortcut for pch_bsize_unwrap(pch_bsize_encode(size))
 * which can be used when the benefits of the type-wrapping of the
 * encoding are not needed.
 */
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

/*! \brief encode a 16-bit value into its pch_bsize_t along
 * with an "exact"
 *  \ingroup picochan_base
 *
 * This encodes n into its pch_bsize_t along with a flag bit that
 * indicates whether decoding the result will produce exactly n.
 *
 * This function is declared as "static inline" to be used in places
 * where it is appropriate to have the code inlined. A corresponding
 * function pch_bsize_encodex is available as an ordinary function.
 */
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

/*! \brief encode a 16-bit value as a pch_bsize_t
 *  \ingroup picochan_base
 *
 * This does the same as pch_bsize_encodex_inline
 * but does not return the exactness.
 */
static inline pch_bsize_t pch_bsize_encode_inline(uint16_t n) {
        return pch_bsize_wrap(pch_bsize_encode_raw_inline(n));
}

/*! \brief decodes a raw bsize-encoded value
 *  \ingroup picochan_base
 *
 * This is a shortcut for pch_bsize_decode(pch_bsize_wrap(esize)
 * which can be used when the benefits of the type-wrapping of the
 * encoding are not needed.
 */
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

/*! \brief decodes a pch_bsize_t as the uint16_t it represents
 *  \ingroup picochan_base
 *
 * This function is declared as "static inline" to be used in places
 * where it is appropriate to have the code inlined. A corresponding
 * function pch_bsize_encodex is available as an ordinary function.
 */
static inline uint16_t pch_bsize_decode_inline(pch_bsize_t bsize) {
        return pch_bsize_decode_raw_inline(bsize.esize);
}

/*! \brief Encode a 16-bit value into its raw 8-bit bsize encoding
 *  \ingroup picochan_base
 */
uint8_t pch_bsize_encode_raw(uint16_t n);

#endif
