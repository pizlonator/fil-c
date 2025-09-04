/* Initialization of libc after dlmopen/dlclose/dlmopen (bug 29528).  Main.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <support/check.h>
#include <support/xdlfcn.h>

/* Run the test multiple times, to check finding a new namespace while
   another namespace is already in use.  This used to trigger bug 29600.  */
static void
recurse (int depth)
{
  if (depth == 0)
    return;

  printf ("info: running at depth %d\n", depth);
  void *handle = xdlmopen (LM_ID_NEWLM, "tst-dlmopen-twice-mod1.so",
                           RTLD_NOW);
  xdlclose (handle);
  handle = xdlmopen (LM_ID_NEWLM, "tst-dlmopen-twice-mod2.so", RTLD_NOW);
  int (*run_check) (void) = xdlsym (handle, "run_check");
  TEST_COMPARE (run_check (), 0);
  recurse (depth - 1);
  xdlclose (handle);
}

static int
do_test (void)
{
  /* First run the test without nesting.  */
  recurse (1);

  /* Then with nesting.  The constant needs to be less than the
     glibc.rtld.nns tunable (which is 4 by default).  */
  recurse (3);
  return 0;
}

#include <support/test-driver.c>
