/* Interposed malloc with dynamic TLS.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <dlfcn.h>

__thread unsigned int malloc_subsytem_counter;

static __typeof (malloc) *malloc_fptr;
static __typeof (free) *free_fptr;
static __typeof (calloc) *calloc_fptr;
static __typeof (realloc) *realloc_fptr;

static void __attribute__ ((constructor))
init (void)
{
  malloc_fptr = dlsym (RTLD_NEXT, "malloc");
  free_fptr = dlsym (RTLD_NEXT, "free");
  calloc_fptr = dlsym (RTLD_NEXT, "calloc");
  realloc_fptr = dlsym (RTLD_NEXT, "realloc");
}

void *
malloc (size_t size)
{
  ++malloc_subsytem_counter;
  return malloc_fptr (size);
}

void
free (void *ptr)
{
  ++malloc_subsytem_counter;
  return free_fptr (ptr);
}

void *
calloc (size_t a, size_t b)
{
  ++malloc_subsytem_counter;
  return calloc_fptr (a, b);
}

void *
realloc (void *ptr, size_t size)
{
  ++malloc_subsytem_counter;
  return realloc_fptr (ptr, size);
}
