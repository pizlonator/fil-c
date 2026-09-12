/* Test for fdopen memory leak without SEEK_END support (bug 31840).
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


#include <errno.h>
#include <fcntl.h>
#include <mcheck.h>
#include <stddef.h>
#include <stdio.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <unistd.h>

static int
do_test (void)
{
  mtrace ();

  /* This file is special because it is seekable, but only
     with SEEK_SET, not SEEK_END.  */
  int fd = open ("/proc/self/mem", O_RDWR);
  if (fd < 0)
    FAIL_UNSUPPORTED ("/proc/self/mem not found: %m");
  FILE *fp = fdopen (fd, "a");
  /* The fdopen call should have failed because it tried to use
     SEEK_END.  */
  TEST_VERIFY (fp == NULL);
  TEST_COMPARE (errno, EINVAL);
  xclose (fd);
  return 0;
}

#include <support/test-driver.c>
