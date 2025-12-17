/*
 * Copyright (C) 2010-2012 Free Software Foundation, Inc.
 * Copyright (C) 2022 Tobias Heider <tobias.heider@canonical.com>
 *
 * Author: Nikos Mavrogiannopoulos
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
#include "errors.h"
#include "num.h"
#include "mpi.h"
#include <nettle/bignum.h> /* includes gmp.h */

static void gnutls_free_zero(void *data, size_t size);
static void *gnutls_realloc_zero(void *data, size_t old_size, size_t new_size);

static void *(*allocfunc)(size_t);
static void *(*reallocfunc)(void *, size_t, size_t);
static void (*freefunc)(void *, size_t);

/* Functions that refer to the initialization of the nettle library.
 */

int gnutls_crypto_init(void)
{
	void *(*defallocfunc)(size_t);
	void *(*defreallocfunc)(void *, size_t, size_t);
	void (*deffreefunc)(void *, size_t);

	/* Check if non-default allocators are being used.
	 * Some applications like guile override GMP allocators
	 * with GC capable alternatives. Do nothing if this is
	 * the case.
	 */
	mp_get_memory_functions(&allocfunc, &reallocfunc, &freefunc);
	mp_set_memory_functions(NULL, NULL, NULL);
	mp_get_memory_functions(&defallocfunc, &defreallocfunc, &deffreefunc);
	if (reallocfunc != defreallocfunc || freefunc != deffreefunc) {
		mp_set_memory_functions(allocfunc, reallocfunc, freefunc);
		return (0);
	}

	/* Overload GMP allocators with safe alternatives */
	mp_set_memory_functions(NULL, gnutls_realloc_zero, gnutls_free_zero);
	return 0;
}

/* Functions that refer to the deinitialization of the nettle library.
 */

void gnutls_crypto_deinit(void)
{
	mp_set_memory_functions(allocfunc, reallocfunc, freefunc);
}

/*-
 * gnutls_free_zero:
 * @data: the memory to free
 * @size: the size of memory
 *
 * This function will operate similarly to free(), but will safely
 * zeroize the memory pointed to by data before freeing.
 *
 -*/
static void gnutls_free_zero(void *data, size_t size)
{
	explicit_bzero(data, size);
	free(data);
}

/*-
 * gnutls_realloc_zero:
 * @data: the memory to free
 * @old_size: the size of memory before reallocation
 * @new_size: the size of memory after reallocation
 *
 * This function will operate similarly to realloc(), but will safely
 * zeroize discarded memory.
 *
 -*/
static void *gnutls_realloc_zero(void *data, size_t old_size, size_t new_size)
{
	void *p;

	if (data == NULL || old_size == 0) {
		p = realloc(data, new_size);
		if (p == NULL)
			abort();
		return p;
	}

	if (new_size == 0) {
		explicit_bzero(data, old_size);
		free(data);
		return NULL;
	}

	if (old_size == new_size)
		return data;

	p = malloc(new_size);
	if (p == NULL) {
		explicit_bzero(data, old_size);
		abort();
	}
	memcpy(p, data, MIN(old_size, new_size));
	explicit_bzero(data, old_size);
	free(data);

	return p;
}
