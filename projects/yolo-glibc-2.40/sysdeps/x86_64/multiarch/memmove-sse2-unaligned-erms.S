/* memmove with SSE2.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

# include "x86-sse2-vecs.h"

# define PREFETCHNT	prefetchnta

# ifndef MEMMOVE_SYMBOL
#  define MEMMOVE_SYMBOL(p,s)	p##_sse2_##s
# endif

# include "multiarch/memmove-vec-unaligned-erms.S"

# if MINIMUM_X86_ISA_LEVEL <= 2
#  include "memmove-shlib-compat.h"
# endif
#endif
