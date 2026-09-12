/* Test that dlopen preserves already accessed TLS (bug 31717).
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

#include <stdbool.h>
#include <support/check.h>
#include <support/xdlfcn.h>
#include <ctype.h>

static int
do_test (void)
{
  void *handle = xdlopen ("tst-dlopen-tlsreinitmod1.so", RTLD_NOW);

  bool *tlsreinitmod3_tested = xdlsym (handle, "tlsreinitmod3_tested");
  TEST_VERIFY (*tlsreinitmod3_tested);

  xdlclose (handle);

  /* This crashes if the libc.so.6 TLS image has been reverted.  */
  TEST_VERIFY (!isupper ('@'));

  return 0;
}

#include <support/test-driver.c>
