/*
 * Copyright (C) 2000-2012 Free Software Foundation, Inc.
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

#ifndef GNUTLS_LIB_MEM_H
#define GNUTLS_LIB_MEM_H

#include "config.h"

#ifdef HAVE_SANITIZER_ASAN_INTERFACE_H
#include <sanitizer/asan_interface.h>
#endif

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

/* These realloc functions will return ptr if size==0, and will free
 * the ptr if the new allocation failed.
 */
void *gnutls_realloc_fast(void *ptr, size_t size);
void *_gnutls_reallocarray_fast(void *ptr, size_t nmemb, size_t size);

char *_gnutls_strdup(const char *);

void *_gnutls_reallocarray(void *, size_t, size_t);

unsigned _gnutls_mem_is_zero(const uint8_t *ptr, unsigned size);

#define zrelease_mpi_key(mpi)             \
	if (*mpi != NULL) {               \
		_gnutls_mpi_clear(*mpi);  \
		_gnutls_mpi_release(mpi); \
	}

#define zeroize_key(x, size) gnutls_memset(x, 0, size)

#define zeroize_temp_key zeroize_key
#define zrelease_temp_mpi_key zrelease_mpi_key

static inline void _gnutls_memory_mark_undefined(void *addr, size_t size)
{
#ifdef HAVE_SANITIZER_ASAN_INTERFACE_H
	ASAN_POISON_MEMORY_REGION(addr, size);
#endif
#ifdef HAVE_VALGRIND_MEMCHECK_H
	if (RUNNING_ON_VALGRIND)
		VALGRIND_MAKE_MEM_UNDEFINED(addr, size);
#endif
}

static inline void _gnutls_memory_mark_defined(void *addr, size_t size)
{
#ifdef HAVE_SANITIZER_ASAN_INTERFACE_H
	ASAN_UNPOISON_MEMORY_REGION(addr, size);
#endif
#ifdef HAVE_VALGRIND_MEMCHECK_H
	if (RUNNING_ON_VALGRIND)
		VALGRIND_MAKE_MEM_DEFINED(addr, size);
#endif
}

#endif /* GNUTLS_LIB_MEM_H */
