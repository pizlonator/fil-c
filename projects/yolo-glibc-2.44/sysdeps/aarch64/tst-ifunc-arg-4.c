/* Test for ifunc resolver that uses __ifunc_hwcap helper function.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdint.h>
#include <sys/auxv.h>
#include <sys/ifunc.h>
#include <support/check.h>

static int
one (void)
{
  return 1;
}

static int
two (void)
{
  return 2;
}

/* Resolver function.  */
static __attribute_used__ void *
resolver (uint64_t arg0, const uint64_t arg1[])
{
  uint64_t hwcap2 = __ifunc_hwcap (_IFUNC_ARG_AT_HWCAP2, arg0, arg1);
  if (hwcap2 & HWCAP2_POE)
    return (void *)one;
  else
    return (void *)two;
}

/* An extern visible ifunc symbol.  */
int fun (void) __attribute__((ifunc ("resolver")));

static int
do_test (void)
{
  if (getauxval (AT_HWCAP2) & HWCAP2_POE)
    {
      printf ("using 1st implementation\n");
      TEST_VERIFY (fun () == 1);
    }
  else
    {
      printf ("using 2nd implementation\n");
      TEST_VERIFY (fun () == 2);
    }
  return 0;
}

#include <support/test-driver.c>
