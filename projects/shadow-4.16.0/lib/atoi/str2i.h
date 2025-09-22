// SPDX-FileCopyrightText: 2007-2009, Nicolas François
// SPDX-FileCopyrightText: 2023-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#ifndef SHADOW_INCLUDE_LIB_ATOI_STR2I_H_
#define SHADOW_INCLUDE_LIB_ATOI_STR2I_H_


#include <config.h>

#include <limits.h>
#include <stddef.h>

#include "atoi/a2i.h"
#include "attr.h"


#define str2i(TYPE, ...)                                                      \
(                                                                             \
	_Generic((TYPE) 0,                                                    \
		short:              str2sh,                                   \
		int:                str2si,                                   \
		long:               str2sl,                                   \
		long long:          str2sll,                                  \
		unsigned short:     str2uh,                                   \
		unsigned int:       str2ui,                                   \
		unsigned long:      str2ul,                                   \
		unsigned long long: str2ull                                   \
	)(__VA_ARGS__)                                                        \
)


ATTR_STRING(2) ATTR_ACCESS(write_only, 1)
inline int str2sh(short *restrict n, const char *restrict s);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1)
inline int str2si(int *restrict n, const char *restrict s);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1)
inline int str2sl(long *restrict n, const char *restrict s);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1)
inline int str2sll(long long *restrict n, const char *restrict s);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1)
inline int str2uh(unsigned short *restrict n, const char *restrict s);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1)
inline int str2ui(unsigned int *restrict n, const char *restrict s);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1)
inline int str2ul(unsigned long *restrict n, const char *restrict s);
ATTR_STRING(2) ATTR_ACCESS(write_only, 1)
inline int str2ull(unsigned long long *restrict n, const char *restrict s);


inline int
str2sh(short *restrict n, const char *restrict s)
{
	return a2sh(n, s, NULL, 0, SHRT_MIN, SHRT_MAX);
}


inline int
str2si(int *restrict n, const char *restrict s)
{
	return a2si(n, s, NULL, 0, INT_MIN, INT_MAX);
}


inline int
str2sl(long *restrict n, const char *restrict s)
{
	return a2sl(n, s, NULL, 0, LONG_MIN, LONG_MAX);
}


inline int
str2sll(long long *restrict n, const char *restrict s)
{
	return a2sll(n, s, NULL, 0, LLONG_MIN, LLONG_MAX);
}


inline int
str2uh(unsigned short *restrict n, const char *restrict s)
{
	return a2uh(n, s, NULL, 0, 0, USHRT_MAX);
}


inline int
str2ui(unsigned int *restrict n, const char *restrict s)
{
	return a2ui(n, s, NULL, 0, 0, UINT_MAX);
}


inline int
str2ul(unsigned long *restrict n, const char *restrict s)
{
	return a2ul(n, s, NULL, 0, 0, ULONG_MAX);
}


inline int
str2ull(unsigned long long *restrict n, const char *restrict s)
{
	return a2ull(n, s, NULL, 0, 0, ULLONG_MAX);
}


#endif  // include guard
