/* Test fwrite on a read-only stream.
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

#include <stdio.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

/* A small buffer size is enough to run this test.  */
#define BUFSIZE 4

static int
do_test (void)
{
  int fd;
  FILE *f;
  struct stat64 st;

  /* Create a temporary file and open it in read-only mode.  */
  fd = create_temp_file ("tst-fwrite-ro", NULL);
  TEST_VERIFY_EXIT (fd != -1);
  f = fdopen (fd, "r");
  TEST_VERIFY_EXIT (f != NULL);

  /* Try to write to the temporary file with nmemb = 0, then check that
     fwrite returns 0.  No errors are expected from this.  */
  TEST_COMPARE (fwrite ("a", 1, 0, f), 0);
  TEST_COMPARE (ferror (f), 0);

  /* Try to write to the temporary file with size = 0, then check that
     fwrite returns 0.  No errors are expected from this.  */
  TEST_COMPARE (fwrite ("a", 0, 1, f), 0);
  TEST_COMPARE (ferror (f), 0);

  /* Try to write a single byte to the temporary file, then check that
     fwrite returns 0.  Check if an error was reported.  */
  TEST_COMPARE (fwrite ("a", 1, 1, f), 0);
  TEST_COMPARE (ferror (f), 1);
  clearerr (f);

  xfstat64 (fd, &st);
  TEST_COMPARE (st.st_size, 0);

  xfclose (f);

  return 0;
}

#include <support/test-driver.c>
