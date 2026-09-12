/* Wrappers for malloc-like functions to allow testing the implementation
   without optimization.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#ifndef TST_MALLOC_AUX_H
#define TST_MALLOC_AUX_H

#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>

static __typeof (aligned_alloc) * volatile aligned_alloc_indirect
  = aligned_alloc;
static __typeof (calloc) * volatile calloc_indirect = calloc;
static __typeof (malloc) * volatile malloc_indirect = malloc;
static __typeof (memalign) * volatile memalign_indirect = memalign;
static __typeof (posix_memalign) * volatile posix_memalign_indirect
  = posix_memalign;
static __typeof (pvalloc) * volatile pvalloc_indirect = pvalloc;
static __typeof (realloc) * volatile realloc_indirect = realloc;
static __typeof (valloc) * volatile valloc_indirect = valloc;
static __typeof (reallocarray) * volatile reallocarray_indirect = reallocarray;

#undef aligned_alloc
#undef calloc
#undef malloc
#undef memalign
#undef posix_memalign
#undef pvalloc
#undef realloc
#undef valloc
#undef reallocarray

#define aligned_alloc aligned_alloc_indirect
#define calloc calloc_indirect
#define malloc malloc_indirect
#define memalign memalign_indirect
#define posix_memalign posix_memalign_indirect
#define pvalloc pvalloc_indirect
#define realloc realloc_indirect
#define valloc valloc_indirect
#define reallocarray reallocarray_indirect

#endif /* TST_MALLOC_AUX_H */
