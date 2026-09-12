/* Test freopen and freopen64 with large offsets.
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
#include <mcheck.h>
#include <stdio.h>
#include <stdlib.h>

#include <support/check.h>
#include <support/descriptors.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xstdio.h>

#define START_TEST(DESC)			\
  do						\
    {						\
      fds = support_descriptors_list ();	\
      verbose_printf (DESC);			\
    }						\
  while (0)

#define END_TEST				\
  do						\
    {						\
      support_descriptors_check (fds);		\
      support_descriptors_free (fds);		\
    }						\
  while (0)

int
do_test (void)
{
  mtrace ();
  struct support_descriptors *fds;
  FILE *fp;
  int ret;

  char *temp_dir = support_create_temp_directory ("tst-freopen5");
  /* This file is removed at the end of each test rather than left
     around between tests to avoid problems with subsequent tests
     reopening it as a large (2GB + 1 byte) file.  */
  char *file1 = xasprintf ("%s/file1", temp_dir);

  /* fopen with freopen64: large offsets OK.  */
  START_TEST ("testing fopen with freopen64\n");
  fp = fopen ("/dev/null", "r");
  TEST_VERIFY_EXIT (fp != NULL);
  fp = freopen64 (file1, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  setbuf (fp, NULL);
  ret = fseeko64 (fp, 1LL << 32, SEEK_SET);
  TEST_COMPARE (ret, 0);
  ret = fputc ('x', fp);
  TEST_COMPARE (ret, 'x');
  xfclose (fp);
  ret = remove (file1);
  TEST_COMPARE (ret, 0);
  END_TEST;

  /* fopen64 with freopen64: large offsets OK.  */
  START_TEST ("testing fopen64 with freopen64\n");
  fp = fopen64 ("/dev/null", "r");
  TEST_VERIFY_EXIT (fp != NULL);
  fp = freopen64 (file1, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  setbuf (fp, NULL);
  ret = fseeko64 (fp, 1LL << 32, SEEK_SET);
  TEST_COMPARE (ret, 0);
  ret = fputc ('x', fp);
  TEST_COMPARE (ret, 'x');
  xfclose (fp);
  ret = remove (file1);
  TEST_COMPARE (ret, 0);
  END_TEST;

  /* fopen with freopen: large offsets not OK on 32-bit systems.  */
  START_TEST ("testing fopen with freopen\n");
  fp = fopen ("/dev/null", "r");
  TEST_VERIFY_EXIT (fp != NULL);
  fp = freopen (file1, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  setbuf (fp, NULL);
  ret = fseeko64 (fp, 1LL << 32, SEEK_SET);
  TEST_COMPARE (ret, 0);
  errno = 0;
  ret = fputc ('x', fp);
  if (sizeof (off_t) == 4)
    {
      TEST_COMPARE (ret, EOF);
      TEST_COMPARE (errno, EFBIG);
    }
  else
    TEST_COMPARE (ret, 'x');
  fclose (fp);
  ret = remove (file1);
  TEST_COMPARE (ret, 0);
  END_TEST;

  /* fopen64 with freopen: large offsets not OK on 32-bit systems.  */
  START_TEST ("testing fopen64 with freopen\n");
  fp = fopen64 ("/dev/null", "r");
  TEST_VERIFY_EXIT (fp != NULL);
  fp = freopen (file1, "w", fp);
  TEST_VERIFY_EXIT (fp != NULL);
  setbuf (fp, NULL);
  ret = fseeko64 (fp, 1LL << 32, SEEK_SET);
  TEST_COMPARE (ret, 0);
  errno = 0;
  ret = fputc ('x', fp);
  if (sizeof (off_t) == 4)
    {
      TEST_COMPARE (ret, EOF);
      TEST_COMPARE (errno, EFBIG);
    }
  else
    TEST_COMPARE (ret, 'x');
  fclose (fp);
  ret = remove (file1);
  TEST_COMPARE (ret, 0);
  END_TEST;

  free (temp_dir);
  free (file1);
  return 0;
}

#include <support/test-driver.c>
