/* Test fread.
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
#include <stdlib.h>
#include <string.h>

#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

int
do_test (void)
{
  char *temp_dir = support_create_temp_directory ("tst-fread");
  char *file1 = xasprintf ("%s/file1", temp_dir);
  support_write_file_string (file1, "file1");
  add_temp_file (file1);
  FILE *fp;
  size_t ret;
  char buf[1024];

  verbose_printf ("test single-byte reads\n");
  fp = xfopen (file1, "r");
  memset (buf, 0, sizeof buf);
  ret = fread (buf, 1, 2, fp);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (buf[0], 'f');
  TEST_COMPARE (buf[1], 'i');
  TEST_COMPARE (feof (fp), 0);
  TEST_COMPARE (ftell (fp), 2);
  memset (buf, 0, sizeof buf);
  ret = fread (buf, 1, 3, fp);
  TEST_COMPARE (ret, 3);
  TEST_COMPARE (buf[0], 'l');
  TEST_COMPARE (buf[1], 'e');
  TEST_COMPARE (buf[2], '1');
  TEST_COMPARE (ftell (fp), 5);
  TEST_COMPARE (feof (fp), 0);
  memset (buf, 0, sizeof buf);
  ret = fread (buf, 1, 1, fp);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE (!!feof (fp), 1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (ftell (fp), 5);
  xfclose (fp);

  verbose_printf ("test single-byte reads, EOF part way through\n");
  fp = xfopen (file1, "r");
  memset (buf, 0, sizeof buf);
  ret = fread (buf, 1, sizeof buf, fp);
  TEST_COMPARE (ret, 5);
  TEST_COMPARE (buf[0], 'f');
  TEST_COMPARE (buf[1], 'i');
  TEST_COMPARE (buf[2], 'l');
  TEST_COMPARE (buf[3], 'e');
  TEST_COMPARE (buf[4], '1');
  TEST_COMPARE (!!feof (fp), 1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (ftell (fp), 5);
  xfclose (fp);

  verbose_printf ("test multi-byte reads\n");
  fp = xfopen (file1, "r");
  memset (buf, 0, sizeof buf);
  ret = fread (buf, 2, 2, fp);
  TEST_COMPARE (ret, 2);
  TEST_COMPARE (buf[0], 'f');
  TEST_COMPARE (buf[1], 'i');
  TEST_COMPARE (buf[2], 'l');
  TEST_COMPARE (buf[3], 'e');
  TEST_COMPARE (feof (fp), 0);
  TEST_COMPARE (ftell (fp), 4);
  memset (buf, 0, sizeof buf);
  ret = fread (buf, 3, 3, fp);
  TEST_COMPARE (ret, 0);
  /* The bytes written for a partial element read are unspecified.  */
  TEST_COMPARE (!!feof (fp), 1);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (ftell (fp), 5);
  xfclose (fp);

  verbose_printf ("test read error\n");
  fp = xfopen (file1, "r");
  xclose (fileno (fp));
  memset (buf, 0, sizeof buf);
  ret = fread (buf, 1, sizeof buf, fp);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE (feof (fp), 0);
  TEST_COMPARE (!!ferror (fp), 1);
  fclose (fp);

  verbose_printf ("test zero size\n");
  fp = xfopen (file1, "r");
  ret = fread (buf, 0, SIZE_MAX, fp);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE (feof (fp), 0);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (ftell (fp), 0);
  xfclose (fp);

  verbose_printf ("test zero items\n");
  fp = xfopen (file1, "r");
  ret = fread (buf, SIZE_MAX, 0, fp);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE (feof (fp), 0);
  TEST_COMPARE (ferror (fp), 0);
  TEST_COMPARE (ftell (fp), 0);
  xfclose (fp);

  free (temp_dir);
  free (file1);
  return 0;
}

#include <support/test-driver.c>
