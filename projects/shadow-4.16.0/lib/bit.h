/*
 * SPDX-FileCopyrightText:  2022 - 2023, Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 */


#ifndef SHADOW_INCLUDE_LIB_BIT_H_
#define SHADOW_INCLUDE_LIB_BIT_H_


#include <config.h>

#include <limits.h>


#ifndef ULONG_WIDTH
#define ULONG_WIDTH (sizeof(unsigned long) * CHAR_BIT)
#endif


inline unsigned long bit_ceilul(unsigned long x);
inline unsigned long bit_ceil_wrapul(unsigned long x);
inline int leading_zerosul(unsigned long x);


/* stdc_bit_ceilul(3) */
inline unsigned long
bit_ceilul(unsigned long x)
{
	return 1 + (ULONG_MAX >> leading_zerosul(x));
}


/* stdc_bit_ceilul(3), but wrap instead of having Undefined Behavior */
inline unsigned long
bit_ceil_wrapul(unsigned long x)
{
	if (x == 0)
		return 0;

	return bit_ceilul(x);
}

/* stdc_leading_zerosul(3) */
inline int
leading_zerosul(unsigned long x)
{
	return (x == 0) ? ULONG_WIDTH : __builtin_clzl(x);
}


#endif  // include guard
