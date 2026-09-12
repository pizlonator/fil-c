/* Module used for improved aligned_alloc testing.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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

#include <libc-symbols.h>
#include <stdlib.h>
#include <time.h>

extern void *__libc_malloc (size_t size);
extern void *__libc_calloc (size_t n, size_t size);

__thread unsigned int seed = 0;

int aligned_alloc_count = 0;
int libc_malloc_count = 0;
int libc_calloc_count = 0;

static void *
get_random_alloc (size_t size)
{
  void *retval;
  size_t align;
  struct timespec tp;

  if (seed == 0)
    {
      clock_gettime (CLOCK_REALTIME, &tp);
      seed = tp.tv_nsec;
    }

  switch (rand_r (&seed) % 3)
    {
    case 1:
      /* Get a random alignment value.  Biased towards the smaller
       * values up to 16384. Must be a power of 2. */
      align = 1 << rand_r (&seed) % 15;
      retval = aligned_alloc (align, size);
      aligned_alloc_count++;
      break;
    case 2:
      retval = __libc_calloc (1, size);
      libc_calloc_count++;
      break;
    default:
      retval = __libc_malloc (size);
      libc_malloc_count++;
      break;
    }

  return retval;
}

void *
__random_malloc (size_t size)
{
  return get_random_alloc (size);
}
strong_alias (__random_malloc, malloc)
