/* A Generic Optimized memcmp implementation for AARCH64.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

/* The actual memcmp code is in ../memcmp.S.  If we are
   building libc this file defines __memcmp_generic. Otherwise
   the include of ../memcmp.S will define the normal __memcmp
   entry points.  */

#include <sysdep.h>

#if IS_IN (libc)

# define MEMCMP __memcmp_generic

/* Do not hide the generic versions of memcmp, we use them
   internally.  */
# undef libc_hidden_builtin_def
# define libc_hidden_builtin_def(name)

# ifdef SHARED
/* It doesn't make sense to send libc-internal memcmp calls through a PLT. */
	.globl __GI_memcmp; __GI_memcmp = __memcmp_generic
# endif

#endif

#include "../memcmp.S"
