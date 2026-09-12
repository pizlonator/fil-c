/* Simple test for BTI support: this base source file is included in
   several tests.
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

#include <sys/auxv.h>
#include <support/check.h>
#include <support/test-driver.h>

#ifndef FUN_ATTRIBUTE
# define FUN_ATTRIBUTE
#endif

/* Defined in tst-bti-mod.c file.  */
extern int fun (void) FUN_ATTRIBUTE;

typedef int (*fun_t) (void);

static int
do_test (void)
{
  unsigned long hwcap2 = getauxval (AT_HWCAP2);
  if ((hwcap2 & HWCAP2_BTI) == 0)
    FAIL_UNSUPPORTED ("BTI is not supported by this system");
  fun_t fn = &fun;
  return fn ();
}

#include <support/test-driver.c>
