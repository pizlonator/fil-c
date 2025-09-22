/*
 * SPDX-FileCopyrightText:  2022 - 2023, Alejandro Colomar <alx@kernel.org>
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 */


#ifndef SHADOW_INCLUDE_LIB_STPECPY_H_
#define SHADOW_INCLUDE_LIB_STPECPY_H_


#include <config.h>

#if !defined(HAVE_STPECPY)

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "attr.h"


ATTR_STRING(3)
inline char *stpecpy(char *dst, char *end, const char *restrict src);


/*
 * SYNOPSIS
 *	[[gnu::null_terminated_string_arg(3)]]
 *	char *_Nullable stpecpy(char *_Nullable dst, char end[0],
 *	                        const char *restrict src);
 *
 * ARGUMENTS
 *	dst	Destination buffer where to copy a string.
 *
 *	end	Pointer to one after the last element of the buffer
 *		pointed to by `dst`.  Usually, it should be calculated
 *		as `dst + NITEMS(dst)`.
 *
 *	src	Source string to be copied into dst.
 *
 * DESCRIPTION
 *	This function copies the string pointed to by src, into a string
 *	at the buffer pointed to by dst.  If the destination buffer,
 *	limited by a pointer to its end --one after its last element--,
 *	isn't large enough to hold the copy, the resulting string is
 *	truncated.
 *
 *	This function can be chained with calls to [v]stpeprintf().
 *
 * RETURN VALUE
 *	dst + strlen(dst)
 *		•  On success, this function returns a pointer to the
 *		   terminating NUL byte.
 *
 *	end
 *		•  If this call truncated the resulting string.
 *		•  If `dst == end` (a previous chained call to these
 *		   functions truncated).
 *	NULL
 *		•  If `dst == NULL` (a previous chained call to
 *		   [v]stpeprintf() failed).
 *
 * ERRORS
 *	This function doesn't set errno.
 */


inline char *
stpecpy(char *dst, char *end, const char *restrict src)
{
	bool    trunc;
	size_t  dsize, dlen, slen;

	if (dst == end)
		return end;
	if (dst == NULL)
		return NULL;

	dsize = end - dst;
	slen = strnlen(src, dsize);
	trunc = (slen == dsize);
	dlen = slen - trunc;

	return stpcpy(mempcpy(dst, src, dlen), "") + trunc;
}


#endif  // !HAVE_STPECPY
#endif  // include guard
