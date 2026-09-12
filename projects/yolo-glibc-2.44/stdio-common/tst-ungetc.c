/* Test for ungetc bugs.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
   Copyright The GNU Toolchain Authors.
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
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

static int
do_test (void)
{
  char *name = NULL;
  FILE *fp = NULL;
  int c;
  char buffer[64];

  int fd = create_temp_file ("tst-ungetc.", &name);
  if (fd == -1)
    FAIL_EXIT1 ("cannot create temporary file: %m");
  xclose (fd);

  fp = xfopen (name, "w");
  fputs ("bla", fp);
  xfclose (fp);

  fp = xfopen (name, "r");
  TEST_VERIFY_EXIT (ungetc ('z', fp) == 'z');
  TEST_VERIFY_EXIT (getc (fp) == 'z');
  TEST_VERIFY_EXIT (getc (fp) == 'b');
  TEST_VERIFY_EXIT (getc (fp) == 'l');
  TEST_VERIFY_EXIT (ungetc ('m', fp) == 'm');
  TEST_VERIFY_EXIT (ungetc ('n', fp) == 'n');
  TEST_VERIFY_EXIT (getc (fp) == 'n');
  TEST_VERIFY_EXIT (getc (fp) == 'm');
  TEST_VERIFY_EXIT ((c = getc (fp)) == 'a');
  TEST_VERIFY_EXIT (getc (fp) == EOF);
  TEST_VERIFY_EXIT (ungetc (c, fp) == c);
  TEST_VERIFY_EXIT (feof (fp) == 0);
  TEST_VERIFY_EXIT (getc (fp) == c);
  TEST_VERIFY_EXIT (getc (fp) == EOF);
  xfclose (fp);

  fp = xfopen (name, "r");
  TEST_VERIFY_EXIT (getc (fp) == 'b');
  TEST_VERIFY_EXIT (getc (fp) == 'l');
  TEST_VERIFY_EXIT (ungetc ('b', fp) == 'b');
  TEST_VERIFY_EXIT (fread (buffer, 1, 64, fp) == 2);
  TEST_VERIFY_EXIT (buffer[0] == 'b');
  TEST_VERIFY_EXIT (buffer[1] == 'a');
  xfclose (fp);

  return 0;
}

#include <support/test-driver.c>
