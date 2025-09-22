// SPDX-FileCopyrightText: 2022-2024, Alejandro Colomar <alx@kernel.org>
// SPDX-License-Identifier: BSD-3-Clause


#ifndef SHADOW_INCLUDE_LIB_STRING_ZUSTR2STP_H_
#define SHADOW_INCLUDE_LIB_STRING_ZUSTR2STP_H_


#include <config.h>

#include <assert.h>
#include <string.h>

#include "must_be.h"
#include "sizeof.h"


/*
 * SYNOPSIS
 *	char *ZUSTR2STP(char *restrict dst, const char src[restrict]);
 *
 * ARGUMENTS
 *	dst	Destination buffer.
 *	src	Source null-padded character sequence.
 *
 * DESCRIPTION
 *	This macro copies at most NITEMS(src) non-null bytes from the
 *	array pointed to by src, followed by a null character, to the
 *	buffer pointed to by dst.
 *
 * RETURN VALUE
 *	dst + strlen(dst)
 *		This function returns a pointer to the terminating NUL
 *		byte.
 *
 * CAVEATS
 *	This function doesn't know the size of the destination buffer.
 *	It assumes it will always be large enough.  Since the size of
 *	the source buffer is known to the caller, it should make sure to
 *	allocate a destination buffer of at least `NITEMS(src) + 1`.
 *
 * EXAMPLES
 *	char  hostname[NITEMS(utmp->ut_host) + 1];
 *
 *	len = ZUSTR2STP(hostname, utmp->ut_host) - hostname;
 *	puts(hostname);
 */


#define ZUSTR2STP(dst, src)                                                   \
({                                                                            \
	static_assert(!is_array(dst) || sizeof(dst) > SIZEOF_ARRAY(src), ""); \
                                                                              \
	stpcpy(mempcpy(dst, src, strnlen(src, NITEMS(src))), "");             \
})


#endif  // include guard
