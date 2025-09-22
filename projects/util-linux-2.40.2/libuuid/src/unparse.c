/*
 * unparse.c -- convert a UUID to string
 *
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * %End-Header%
 */

#include <stdio.h>
#include "c.h"

#include "uuidP.h"

static char const hexdigits_lower[16] = "0123456789abcdef";
static char const hexdigits_upper[16] = "0123456789ABCDEF";

static void uuid_fmt(const uuid_t uuid, char *buf, char const *restrict fmt)
{
	char *p = buf;
	int i;

	for (i = 0; i < 16; i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10) {
			*p++ = '-';
		}
		size_t tmp = uuid[i];
		*p++ = fmt[tmp >> 4];
		*p++ = fmt[tmp & 15];
	}
	*p = '\0';
}

void uuid_unparse_lower(const uuid_t uu, char *out)
{
	uuid_fmt(uu, out, hexdigits_lower);
}

void uuid_unparse_upper(const uuid_t uu, char *out)
{
	uuid_fmt(uu, out, hexdigits_upper);
}

void uuid_unparse(const uuid_t uu, char *out)
{
#ifdef UUID_UNPARSE_DEFAULT_UPPER
	uuid_fmt(uu, out, hexdigits_upper);
#else
	uuid_fmt(uu, out, hexdigits_lower);
#endif
}
