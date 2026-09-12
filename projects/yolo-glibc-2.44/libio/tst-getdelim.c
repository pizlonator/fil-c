/* Test getdelim conforming to POSIX specifications.

   Note: Most getdelim use cases are covered by stdio-common/tst-getline.

   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
#include <errno.h>
#include <string.h>

#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <support/xstdio.h>
#include <support/temp_file.h>

static int
do_test (void)
{
  /* Check that getdelim sets error indicator on error (BZ #29917) */
  clearerr (stdin);
  TEST_VERIFY (getdelim (0, 0, '\n', stdin) == -1);
  TEST_VERIFY (ferror (stdin) != 0);
  TEST_VERIFY (errno == EINVAL);

  /* Test getdelim with NUL as delimiter */
  verbose_printf ("Testing NUL delimiter\n");
  char *lineptr = NULL;
  size_t linelen = 0;
  char membuf[] = "abc\0d\nef\0";
  FILE *memstream = xfmemopen (membuf, sizeof (membuf), "r");
  TEST_VERIFY (getdelim (&lineptr, &linelen, '\0', memstream) != -1);
  TEST_COMPARE_BLOB (lineptr, 4, "abc\0", 4);
  TEST_VERIFY (getdelim (&lineptr, &linelen, '\0', memstream) != -1);
  TEST_COMPARE_BLOB (lineptr, 5, "d\nef\0", 5);
  xfclose (memstream);
  free (lineptr);

  /* Test that we null-terminate the buffer upon allocating it (BZ #28038).  */
  lineptr = NULL;
  linelen = 0;
  char *file_name;
  TEST_VERIFY_EXIT (create_temp_file ("tst-getdelim.", &file_name) != -1);
  FILE *fp = fopen (file_name, "w+");
  free (file_name);
  TEST_VERIFY_EXIT (fp != NULL);
  TEST_VERIFY (getdelim (&lineptr, &linelen, '\n', fp) == -1);
  TEST_VERIFY (feof (fp));
  TEST_VERIFY (linelen > 0);
  TEST_VERIFY (lineptr[0] == '\0');
  free (lineptr);

  /* Test that we can read until -1 is returned and then access the last line
     of the file.  This behavior was broken by commit
     33eff78c8b28adc4963987880e10d96761f2a167 and later fixed.  */
  lineptr = NULL;
  linelen = 0;
  char input[] = "a\nb\nc\n";
  TEST_VERIFY_EXIT (fwrite (input, 1, sizeof input - 1, fp)
                    == sizeof input - 1);
  TEST_VERIFY_EXIT (fseeko (fp, 0, SEEK_SET) == 0);
  char expect[] = { 'a' - 1, '\n', '\0' };
  for (int i = 0; i < 3; ++i)
    {
      ++expect[0];
      TEST_VERIFY (getdelim (&lineptr, &linelen, '\n', fp) == 2);
      TEST_VERIFY (linelen > 2);
      TEST_VERIFY (strcmp (lineptr, expect) == 0);
    }
  TEST_VERIFY (getdelim (&lineptr, &linelen, '\n', fp) == -1);
  TEST_VERIFY (feof (fp));
  TEST_VERIFY (linelen > 2);
  TEST_VERIFY (strcmp (lineptr, expect) == 0);

  /* Test the same thing without a newline.  */
  TEST_VERIFY_EXIT (fwrite ("d", 1, 1, fp) == 1);
  TEST_VERIFY_EXIT (fseeko (fp, -1, SEEK_CUR) == 0);
  TEST_VERIFY (getdelim (&lineptr, &linelen, '\n', fp) == 1);
  TEST_VERIFY (linelen > 2);
  TEST_VERIFY (strcmp (lineptr, "d") == 0);
  TEST_VERIFY (getdelim (&lineptr, &linelen, '\n', fp) == -1);
  TEST_VERIFY (feof (fp));
  TEST_VERIFY (linelen > 2);
  TEST_VERIFY (strcmp (lineptr, "d") == 0);
  fclose (fp);
  free (lineptr);

  return 0;
}

#include <support/test-driver.c>
