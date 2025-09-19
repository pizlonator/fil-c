/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2013-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "config.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define bit(x_) (1UL << (x_))
#define NBITS(b) (b * 8)
#define LONG_BITS (sizeof(long) * 8)
#define NLONGS(x) (((x) + LONG_BITS - 1) / LONG_BITS)
#define NCHARS(x) ((size_t)(((x) + 7) / 8))

/* This bitfield helper implementation is taken from from libevdev-util.h,
 * except that it has been modified to work with arrays of unsigned chars
 */

static inline bool
bit_is_set(const unsigned char *array, int bit)
{
	return !!(array[bit / 8] & (1 << (bit % 8)));
}

static inline void
set_bit(unsigned char *array, int bit)
{
	array[bit / 8] |= (1 << (bit % 8));
}

static inline void
clear_bit(unsigned char *array, int bit)
{
	array[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool
long_bit_is_set(const unsigned long *array, int bit)
{
	return !!(array[bit / LONG_BITS] & (1ULL << (bit % LONG_BITS)));
}

static inline void
long_set_bit(unsigned long *array, int bit)
{
	array[bit / LONG_BITS] |= (1ULL << (bit % LONG_BITS));
}

static inline void
long_clear_bit(unsigned long *array, int bit)
{
	array[bit / LONG_BITS] &= ~(1ULL << (bit % LONG_BITS));
}

static inline void
long_set_bit_state(unsigned long *array, int bit, int state)
{
	if (state)
		long_set_bit(array, bit);
	else
		long_clear_bit(array, bit);
}

static inline bool
long_any_bit_set(unsigned long *array, size_t size)
{
	unsigned long i;

	assert(size > 0);

	for (i = 0; i < size; i++)
		if (array[i] != 0)
			return true;
	return false;
}

/* A wrapper around a bit mask to avoid type confusion */
typedef struct {
	uint32_t mask;
} bitmask_t;

static inline uint32_t
bitmask_as_u32(bitmask_t mask)
{
	return mask.mask;
}

static inline bool
bitmask_is_empty(bitmask_t mask)
{
	return mask.mask == 0;
}

static inline bool
bitmask_any(bitmask_t mask, bitmask_t bits)
{
	return !!(mask.mask & bits.mask);
}

static inline bool
bitmask_all(bitmask_t mask, bitmask_t bits)
{
	return bits.mask != 0 && (mask.mask & bits.mask) == bits.mask;
}

static inline bool
bitmask_merge(bitmask_t *mask, bitmask_t bits)
{
	bool all = bitmask_all(*mask, bits);

	mask->mask |= bits.mask;

	return all;
}

static inline bool
bitmask_clear(bitmask_t *mask, bitmask_t bits)
{
	bool all = bitmask_all(*mask, bits);

	mask->mask &= ~bits.mask;

	return all;
}

static inline bool
bitmask_bit_is_set(bitmask_t mask, unsigned int bit)
{
	return !!(mask.mask & bit(bit));
}

static inline bool
bitmask_set_bit(bitmask_t *mask, unsigned int bit)
{
	bool isset = bitmask_bit_is_set(*mask, bit);
	mask->mask |= bit(bit);
	return isset;
}

static inline bool
bitmask_clear_bit(bitmask_t *mask, unsigned int bit)
{
	bool isset = bitmask_bit_is_set(*mask, bit);
	mask->mask &= ~bit(bit);
	return isset;
}

static inline bitmask_t
bitmask_new(void)
{
	bitmask_t m = { 0 };
	return m;
}

static inline bitmask_t
bitmask_from_bit(unsigned int bit)
{
	bitmask_t m = { .mask = bit(bit) };
	return m;
}

static inline bitmask_t
bitmask_from_u32(uint32_t mask)
{
	bitmask_t m = { .mask = mask };
	return m;
}

static inline bitmask_t
_bitmask_from_masks(uint32_t mask1, ...)
{
	uint32_t mask = mask1;
	va_list args;
	va_start(args, mask1);

	uint32_t v = va_arg(args, unsigned int);
	while (v != 0) {
		mask |= v;
		v = va_arg(args, unsigned int);
	}
	va_end(args);

	return bitmask_from_u32(mask);
}

#define bitmask_from_masks(...) \
	_bitmask_from_masks(__VA_ARGS__, 0)

static inline bitmask_t
_bitmask_from_bits(unsigned int bit1, ...)
{
	uint32_t mask = bit(bit1);
	va_list args;
	va_start(args, bit1);

	uint32_t v = va_arg(args, unsigned int);
	while (v < 32) {
		mask |= bit(v);
		v = va_arg(args, unsigned int);
	}
	va_end(args);

	return bitmask_from_u32(mask);
}

#define bitmask_from_bits(...) \
	_bitmask_from_bits(__VA_ARGS__, 32)
