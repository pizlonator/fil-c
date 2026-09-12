/* Test that IRELATIVE resolvers may call PLT functions via dlopen + RTLD_NOW.
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


/* Same as tst-ifunc-plt-dlopen, but uses RTLD_NOW to force eager
   relocation of the loaded library.  This exercises the dlopen entry
   point along the eager-binding path.  */

#include <support/xdlfcn.h>
#include <support/check.h>

typedef int (*fn_t) (void);

static int
do_test (void)
{
  void *handle = xdlopen ("tst-ifunc-plt-lib.so", RTLD_NOW | RTLD_LOCAL);

  fn_t compute_a = (fn_t) xdlsym (handle, "compute_a");
  TEST_COMPARE (compute_a (), 1);

  fn_t compute_b = (fn_t) xdlsym (handle, "compute_b");
  TEST_COMPARE (compute_b (), 2);

  xdlclose (handle);

  return 0;
}

#include <support/test-driver.c>
