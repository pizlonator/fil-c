/*	$NetBSD: bitops.h,v 1.11 2012/12/07 02:27:58 christos Exp $	*/

/*-
 * Copyright (c) 2007, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and Joerg Sonnenberger.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef COMPAT_BITOPS_H
#define COMPAT_BITOPS_H

#include <stdint.h>
#include "common.h"

/*
 * Find First Set functions
 */
#ifndef ffs32
static inline int __unused
ffs32(uint32_t _n)
{
	int _v;

	if (!_n)
		return 0;

	_v = 1;
	if ((_n & 0x0000FFFFU) == 0) {
		_n >>= 16;
		_v += 16;
	}
	if ((_n & 0x000000FFU) == 0) {
		_n >>= 8;
		_v += 8;
	}
	if ((_n & 0x0000000FU) == 0) {
		_n >>= 4;
		_v += 4;
	}
	if ((_n & 0x00000003U) == 0) {
		_n >>= 2;
		_v += 2;
	}
	if ((_n & 0x00000001U) == 0) {
		//_n >>= 1;
		_v += 1;
	}
	return _v;
}
#endif

#ifndef ffs64
static inline int __unused
ffs64(uint64_t _n)
{
	int _v;

	if (!_n)
		return 0;

	_v = 1;
	if ((_n & 0x00000000FFFFFFFFULL) == 0) {
		_n >>= 32;
		_v += 32;
	}
	if ((_n & 0x000000000000FFFFULL) == 0) {
		_n >>= 16;
		_v += 16;
	}
	if ((_n & 0x00000000000000FFULL) == 0) {
		_n >>= 8;
		_v += 8;
	}
	if ((_n & 0x000000000000000FULL) == 0) {
		_n >>= 4;
		_v += 4;
	}
	if ((_n & 0x0000000000000003ULL) == 0) {
		_n >>= 2;
		_v += 2;
	}
	if ((_n & 0x0000000000000001ULL) == 0) {
		//_n >>= 1;
		_v += 1;
	}
	return _v;
}
#endif

/*
 * Find Last Set functions
 */
#ifndef fls32
static __inline int __unused
fls32(uint32_t _n)
{
	int _v;

	if (!_n)
		return 0;

	_v = 32;
	if ((_n & 0xFFFF0000U) == 0) {
		_n <<= 16;
		_v -= 16;
	}
	if ((_n & 0xFF000000U) == 0) {
		_n <<= 8;
		_v -= 8;
	}
	if ((_n & 0xF0000000U) == 0) {
		_n <<= 4;
		_v -= 4;
	}
	if ((_n & 0xC0000000U) == 0) {
		_n <<= 2;
		_v -= 2;
	}
	if ((_n & 0x80000000U) == 0) {
		//_n <<= 1;
		_v -= 1;
	}
	return _v;
}
#endif

#ifndef fls64
static int __unused
fls64(uint64_t _n)
{
	int _v;

	if (!_n)
		return 0;

	_v = 64;
	if ((_n & 0xFFFFFFFF00000000ULL) == 0) {
		_n <<= 32;
		_v -= 32;
	}
	if ((_n & 0xFFFF000000000000ULL) == 0) {
		_n <<= 16;
		_v -= 16;
	}
	if ((_n & 0xFF00000000000000ULL) == 0) {
		_n <<= 8;
		_v -= 8;
	}
	if ((_n & 0xF000000000000000ULL) == 0) {
		_n <<= 4;
		_v -= 4;
	}
	if ((_n & 0xC000000000000000ULL) == 0) {
		_n <<= 2;
		_v -= 2;
	}
	if ((_n & 0x8000000000000000ULL) == 0) {
		//_n <<= 1;
		_v -= 1;
	}
	return _v;
}
#endif

#endif /* COMPAT_BITOPS_H_ */
