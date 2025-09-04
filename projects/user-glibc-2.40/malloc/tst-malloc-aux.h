/* Wrappers for malloc-like functions to allow testing the implementation
   without optimization.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

static void *(*volatile aligned_alloc_indirect)(size_t, size_t) = aligned_alloc;
static void *(*volatile calloc_indirect)(size_t, size_t) = calloc;
static void *(*volatile malloc_indirect)(size_t) = malloc;
static void *(*volatile realloc_indirect)(void*, size_t) = realloc;

#undef aligned_alloc
#undef calloc
#undef malloc
#undef realloc

#define aligned_alloc aligned_alloc_indirect
#define calloc calloc_indirect
#define malloc malloc_indirect
#define realloc realloc_indirect

#endif /* TST_MALLOC_AUX_H */
