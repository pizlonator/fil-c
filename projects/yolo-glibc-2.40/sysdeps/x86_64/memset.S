/* memset -- set memory area to CH/0
   Optimized version for x86-64.
   Copyright (C) 2002-2024 Free Software Foundation, Inc.
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

#include <sysdep.h>

#define MEMSET_SYMBOL(p,s)	memset
#define MEMSET_CHK_SYMBOL(p,s)	p

#define WMEMSET_SYMBOL(p,s)	__wmemset
#define WMEMSET_CHK_SYMBOL(p,s) p

#define DEFAULT_IMPL_V1	"multiarch/memset-sse2-unaligned-erms.S"
#define DEFAULT_IMPL_V3	"multiarch/memset-avx2-unaligned-erms.S"
#define DEFAULT_IMPL_V4	"multiarch/memset-evex-unaligned-erms.S"

#include "isa-default-impl.h"

libc_hidden_builtin_def (memset)
libc_hidden_builtin_def (__memset_chk)

#if IS_IN (libc)
libc_hidden_def (__wmemset)
weak_alias (__wmemset, wmemset)
libc_hidden_weak (wmemset)
#endif
