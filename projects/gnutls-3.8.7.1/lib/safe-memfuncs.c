/*
 * Copyright (C) 2014 Red Hat
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

#include "gnutls_int.h"
#include <string.h>

/**
 * gnutls_memset:
 * @data: the memory to set
 * @c: the constant byte to fill the memory with
 * @size: the size of memory
 *
 * This function will operate similarly to memset(), but will
 * not be optimized out by the compiler.
 *
 * Since: 3.4.0
 **/
void gnutls_memset(void *data, int c, size_t size)
{
	explicit_bzero(data, size);
	memset(data, c, size);
}

/**
 * gnutls_memcmp:
 * @s1: the first address to compare
 * @s2: the second address to compare
 * @n: the size of memory to compare
 *
 * This function will operate similarly to memcmp(), but will operate
 * on time that depends only on the size of the string. That is will
 * not return early if the strings don't match on the first byte.
 *
 * Returns: non zero on difference and zero if the buffers are identical.
 *
 * Since: 3.4.0
 **/
int gnutls_memcmp(const void *s1, const void *s2, size_t n)
{
	unsigned i;
	unsigned status = 0;
	const uint8_t *_s1 = s1;
	const uint8_t *_s2 = s2;

	for (i = 0; i < n; i++) {
		status |= (_s1[i] ^ _s2[i]);
	}

	return status;
}

#ifdef TEST_SAFE_MEMSET
int main(void)
{
	char x[64];

	gnutls_memset(x, 0, sizeof(x));

	return 0;
}

#endif
