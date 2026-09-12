/* extra tls utils for the dynamic linker.  Generic stub version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _DL_EXTRA_TLS_H
#define _DL_EXTRA_TLS_H 1
#include <stddef.h>

/* In this generic version, the extra TLS block is unused.  */

/* Returns the size of the extra TLS block, it must always be a multiple of the
   alignment.  */
static inline size_t
_dl_extra_tls_get_size (void)
{
	return 0;
}

/* Returns the alignment requirements of the extra TLS block.  */
static inline size_t
_dl_extra_tls_get_align (void)
{
	return 0;
}

/* Record the offset of the extra TLS block from the thread pointer.  */
static inline void
_dl_extra_tls_set_offset (ptrdiff_t tls_offset __attribute__ ((unused)))
{
}

#endif
