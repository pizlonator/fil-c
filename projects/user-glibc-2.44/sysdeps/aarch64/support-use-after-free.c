/* Support functions for testing malloc: aarch64 version.
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

#include "test-pointer.h"

#include <sys/ifunc.h>
#include <sys/auxv.h>

/* This version clears bits 59:56 (4 bits) to remove possible
   MTE tag from the pointer without trying to access memory
   that this pointer points to.  */
static void *ptr_after_free_mte (void *ptr)
{
  return (void *)((uintptr_t)ptr & ~(0xfull << 56ull));
}

static void *ptr_after_free_generic (void *ptr)
{
  return ptr;
}

static void * __attribute__ ((unused))
ptr_after_free_resolver (unsigned long a0, const unsigned long *a1)
{
  unsigned long hwcap2 = __ifunc_hwcap (_IFUNC_ARG_AT_HWCAP2, a0, a1);
  if (hwcap2 & HWCAP2_MTE)
    return (void *)ptr_after_free_mte;
  return (void *)ptr_after_free_generic;
}

void *support_ptr_after_free (void *ptr)
__attribute__ ((ifunc ("ptr_after_free_resolver")));
