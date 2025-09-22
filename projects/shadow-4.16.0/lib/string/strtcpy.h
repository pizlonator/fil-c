/*
 * SPDX-FileCopyrightText: 2023, Alejandro Colomar <alx@kernel.org>
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef SHADOW_INCLUDE_LIB_STRTCPY_H_
#define SHADOW_INCLUDE_LIB_STRTCPY_H_


#include <config.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include "attr.h"
#include "defines.h"
#include "sizeof.h"


/*
 * SYNOPSIS
 *	[[gnu::null_terminated_string_arg(2)]]
 *	int STRTCPY(char dst[restrict], const char *restrict src);
 *
 * ARGUMENTS
 *	dst	Destination buffer where to copy a string.
 *	src	Source string to be copied into dst.
 *
 * DESCRIPTION
 *	This macro copies the string pointed to by src, into a string
 *	at the buffer pointed to by dst.  If the destination buffer,
 *	isn't large enough to hold the copy, the resulting string is
 *	truncated.  The size of the buffer is calculated internally via
 *	NITEMS().
 *
 * RETURN VALUE
 *	-1	If this call truncated the resulting string.
 *
 *	strlen(dst)
 *		On success.
 *
 * ERRORS
 *	This function doesn't set errno.
 */


#define STRTCPY(dst, src)  strtcpy(dst, src, NITEMS(dst))


ATTR_STRING(2)
inline ssize_t strtcpy(char *restrict dst, const char *restrict src,
    size_t dsize);


inline ssize_t
strtcpy(char *restrict dst, const char *restrict src, size_t dsize)
{
	bool    trunc;
	size_t  dlen, slen;

	if (dsize == 0)
		return -1;

	slen = strnlen(src, dsize);
	trunc = (slen == dsize);
	dlen = slen - trunc;

	stpcpy(mempcpy(dst, src, dlen), "");

	if (trunc)
		return -1;

	return slen;
}


#endif  // include guard
