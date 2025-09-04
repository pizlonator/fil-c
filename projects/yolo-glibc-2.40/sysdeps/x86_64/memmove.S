/* Optimized memmove for x86-64.
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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


#ifdef USE_MULTIARCH
# if !IS_IN (libc)
#  define MEMCPY_SYMBOL(p,s)		memcpy
# endif
#else
# if defined SHARED && IS_IN (libc)
#  define MEMCPY_SYMBOL(p,s)		__memcpy
# else
#  define MEMCPY_SYMBOL(p,s)		memcpy
# endif
#endif
#if !defined USE_MULTIARCH || !IS_IN (libc)
# define MEMPCPY_SYMBOL(p,s)		__mempcpy
#endif

#define MEMMOVE_CHK_SYMBOL(p,s)	p
#define MEMMOVE_SYMBOL(p,s)	memmove


#define DEFAULT_IMPL_V1	"multiarch/memmove-sse2-unaligned-erms.S"
#define DEFAULT_IMPL_V3	"multiarch/memmove-avx-unaligned-erms.S"
#define DEFAULT_IMPL_V4	"multiarch/memmove-evex-unaligned-erms.S"

#include "isa-default-impl.h"

weak_alias (__mempcpy, mempcpy)

#ifndef USE_MULTIARCH
libc_hidden_builtin_def (memmove)
libc_hidden_builtin_def (__memmove_chk)
libc_hidden_builtin_def (__memcpy_chk)
libc_hidden_builtin_def (__mempcpy_chk)
# if defined SHARED && IS_IN (libc)
strong_alias (memmove, __memcpy)
libc_hidden_ver (memmove, memcpy)
# endif
libc_hidden_def (__mempcpy)
weak_alias (__mempcpy, mempcpy)
libc_hidden_builtin_def (mempcpy)


# if defined SHARED && IS_IN (libc)
#  undef memcpy
#  include <shlib-compat.h>
versioned_symbol (libc, __memcpy, memcpy, GLIBC_2_14);
# endif
#endif
