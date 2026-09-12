/* Test flushing input file after ungetc (bug 5994).
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

#include <support/check.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

int
do_test (void)
{
  char *filename = NULL;
  int fd = create_temp_file ("tst-ungetc-fflush", &filename);
  TEST_VERIFY_EXIT (fd != -1);
  xclose (fd);

  /* Test as in bug 5994.  */
  FILE *fp = xfopen (filename, "w");
  TEST_VERIFY_EXIT (fputs ("#include", fp) >= 0);
  xfclose (fp);
  fp = xfopen (filename, "r");
  TEST_COMPARE (fgetc (fp), '#');
  TEST_COMPARE (fgetc (fp), 'i');
  TEST_COMPARE (ungetc ('@', fp), '@');
  TEST_COMPARE (fflush (fp), 0);
  TEST_COMPARE (lseek (fileno (fp), 0, SEEK_CUR), 1);
  TEST_COMPARE (fgetc (fp), 'i');
  TEST_COMPARE (fgetc (fp), 'n');
  xfclose (fp);

  /* Test as in bug 12799 (duplicate of 5994).  */
  fp = xfopen (filename, "w+");
  TEST_VERIFY_EXIT (fputs ("hello world", fp) >= 0);
  rewind (fp);
  TEST_VERIFY (fileno (fp) >= 0);
  char buffer[10];
  TEST_COMPARE (fread (buffer, 1, 5, fp), 5);
  TEST_COMPARE (fgetc (fp), ' ');
  TEST_COMPARE (ungetc ('@', fp), '@');
  TEST_COMPARE (fflush (fp), 0);
  TEST_COMPARE (fgetc (fp), ' ');
  xfclose (fp);

  return 0;
}

#include <support/test-driver.c>
