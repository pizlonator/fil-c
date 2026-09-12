/* Test fflush after ungetc on files using mmap (bug 32535).
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
  int fd = create_temp_file ("tst-fflush-mmap", &filename);
  TEST_VERIFY_EXIT (fd != -1);
  xclose (fd);

  /* Test fflush after ungetc (bug 32535).  */
  FILE *fp = xfopen (filename, "w");
  TEST_VERIFY (0 <= fputs ("test", fp));
  xfclose (fp);

  fp = xfopen (filename, "rm");
  TEST_COMPARE (fgetc (fp), 't');
  TEST_COMPARE (ungetc ('u', fp), 'u');
  TEST_COMPARE (fflush (fp), 0);
  TEST_COMPARE (fgetc (fp), 't');
  TEST_COMPARE (fgetc (fp), 'e');
  xfclose (fp);

  return 0;
}

#include <support/test-driver.c>
