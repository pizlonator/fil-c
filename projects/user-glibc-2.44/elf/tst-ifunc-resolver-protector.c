/* Check that a stack-protector-instrumented IFUNC resolver works in a
   dynamically-linked binary.  BZ #27582.
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

/* Dynamic counterpart to tst-ifunc-resolver-protector-static.

   The dynamic linker calls security_init() before the main relocation
   loop, so a stack-protected resolver fired from startup should observe
   an initialised canary -- but having a regression test guards against
   any future reordering that would break that invariant.  */

#include <support/check.h>

#define SENTINEL 0x5A5A1234

extern int compute (int);
extern int get_resolver_ran (void);

static int
do_test (void)
{
  TEST_COMPARE (compute (1), 1 + SENTINEL);
  TEST_VERIFY (get_resolver_ran () != 0);
  return 0;
}

#include <support/test-driver.c>
