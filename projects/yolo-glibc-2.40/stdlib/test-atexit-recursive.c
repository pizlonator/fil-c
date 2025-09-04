/* Support file for atexit/exit, etc. race tests (BZ #27749).
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

/* Check that atexit handler registered from another handler still called. */

#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <sys/wait.h>
#include <unistd.h>

static void
atexit_cb (void)
{
}

static void
atexit_last (void)
{
  _exit (1);
}

static void
atexit_recursive (void)
{
  atexit (&atexit_cb);
  atexit (&atexit_last);
}

_Noreturn static void
test_and_exit (int count)
{
  for (int i = 0; i < count; ++i)
    atexit (&atexit_cb);
  atexit (&atexit_recursive);
  exit (0);
}

static int
do_test (void)
{
  for (int i = 0; i < 100; ++i)
    if (xfork () == 0)
      test_and_exit (i);

  for (int i = 0; i < 100; ++i)
    {
      int status;
      xwaitpid (0, &status, 0);
      if (!WIFEXITED (status))
	FAIL_EXIT1 ("Failed iterations %d", i);
      TEST_COMPARE (WEXITSTATUS (status), 1);
    }

  return 0;
}

#define TEST_FUNCTION do_test
#include <support/test-driver.c>
