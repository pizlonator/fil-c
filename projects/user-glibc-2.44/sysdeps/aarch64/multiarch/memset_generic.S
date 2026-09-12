/* Memset for aarch64, default version for internal use.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.

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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>

#if IS_IN (libc)
# define MEMSET __memset_generic

/* Do not hide the generic version of memset, we use it internally.  */
# undef libc_hidden_builtin_def
# define libc_hidden_builtin_def(name)

/* Add a hidden definition for use within libc.so.  */
# ifdef SHARED
	.globl __GI_memset; __GI_memset = __memset_generic
# endif
#endif

#include <../memset.S>

#if IS_IN (rtld)
strong_alias (memset, __memset_generic)
#endif
