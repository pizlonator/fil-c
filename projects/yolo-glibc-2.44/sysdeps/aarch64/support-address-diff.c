/* Support functions for pointer arithmetic: aarch64 version.
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
#include <libc-pointer-arith.h>

static ptrdiff_t
address_diff_mte (const void *lhs, const void *rhs)
{
  register const void *x0 asm ("x0") = lhs;
  register const void *x1 asm ("x1") = rhs;
  asm (".inst 0x9ac10000 /* subp x0, x0, x1 */" : "+r" (x0) : "r" (x1));
  return (ptrdiff_t)x0;
}

static ptrdiff_t
address_diff_generic (const void *lhs, const void *rhs)
{
  return PTR_DIFF (lhs, rhs);
}

static void * __attribute__ ((unused))
address_diff_resolver (unsigned long a0, const unsigned long *a1)
{
  unsigned long hwcap2 = __ifunc_hwcap (_IFUNC_ARG_AT_HWCAP2, a0, a1);
  if (hwcap2 & HWCAP2_MTE)
    return (void *)address_diff_mte;
  return (void *)address_diff_generic;
}

ptrdiff_t support_address_diff (const void *lhs, const void *rhs)
__attribute__ ((ifunc ("address_diff_resolver")));
