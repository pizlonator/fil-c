/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2024 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"

unsigned FAST_FUNC bb_popcnt_32(uint32_t m)
{
	/* replace each 2 bit group with the count of set bits in it */
	/* 00->00 01->01 10->01 11->10 */
	m = m - ((m >> 1) & 0x55555555);
	/* in each 4 bit group, add two 2-bit counts */
	m = (m & 0x33333333) + ((m >> 2) & 0x33333333);
	/* in each 8 bit group, add two 4-bit counts (in fact, 3-bit, 0nnn with n=0..4) */
	m = (m + (m >> 4)) & 0x0f0f0f0f;
#if 1 /* assume 32*32->32 multiply is fast */
	m = m * 0x01010101; /* top byte = m + (m<<8) + (m<<16) + (m<<24) */
	return m >> 24;
#else
	/*   0000aaaa0000bbbb0000cccc0000dddd */
	/* +         0000aaaa0000bbbb0000cccc */
	/* = 0000xxxx000_a+b_000xxxxx000_c+d_ (we don't care about x bits) */
	m += m >> 8; /* in each 16-bit group, lowest 5 bits is the count */
	/*   0000xxxx000_a+b_000xxxxx000_c+d_ */
	/* +                 0000xxxx000_a+b_ */
	/* = 0000xxxx000xxxxx00xxxxxx00a+b+cd */
	m += m >> 16; /* in each 32-bit group, lowest 6 bits is the count */
	return m & 0x3f; /* clear x bits */
#endif
}

#if ULONG_MAX > 0xffffffff
unsigned FAST_FUNC bb_popcnt_long(unsigned long m)
{
	BUILD_BUG_ON(sizeof(m) != 8);
	/* 64-bit version of bb_popcnt_32 exists, but it uses 64-bit constants,
	 * which are awkward to generate on assembly level on most CPUs.
	 * For now, just add two 32-bit counts:
	 */
	return bb_popcnt_32((uint32_t)m) + bb_popcnt_32((uint32_t)(m >> 32));
}
#endif
