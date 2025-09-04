/* memset with SSE2.
   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2014-2024 Free Software Foundation, Inc.
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

#include <isa-level.h>

/* MINIMUM_X86_ISA_LEVEL <= 2 because there is no V2 implementation
   so we need this to build for ISA V2 builds. */
#if ISA_SHOULD_BUILD (2)

# include <sysdep.h>
# define USE_WITH_SSE2	1

# include "x86-sse2-vecs.h"

# define MEMSET_SET_VEC0_AND_SET_RETURN(d, r) \
  movd d, %xmm0; \
  movq r, %rax; \
  punpcklbw %xmm0, %xmm0; \
  punpcklwd %xmm0, %xmm0; \
  pshufd $0, %xmm0, %xmm0

# define WMEMSET_SET_VEC0_AND_SET_RETURN(d, r) \
  movd d, %xmm0; \
  pshufd $0, %xmm0, %xmm0; \
  movq r, %rax

# define MEMSET_VDUP_TO_VEC0_HIGH()
# define MEMSET_VDUP_TO_VEC0_LOW()

# define WMEMSET_VDUP_TO_VEC0_HIGH()
# define WMEMSET_VDUP_TO_VEC0_LOW()

# ifndef MEMSET_SYMBOL
#  define MEMSET_SYMBOL(p,s)	p##_sse2_##s
# endif

# ifndef WMEMSET_SYMBOL
#  define WMEMSET_SYMBOL(p,s)	p##_sse2_##s
# endif

# include "memset-vec-unaligned-erms.S"

#endif
