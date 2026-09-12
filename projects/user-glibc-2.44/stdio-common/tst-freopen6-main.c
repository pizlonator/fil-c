/* Test freopen of stdin / stdout / stderr.
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
#include <unistd.h>

#include <support/check.h>
#include <support/file_contents.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xstdio.h>

int
do_test (void)
{
  mtrace ();
  char *temp_dir = support_create_temp_directory ("tst-freopen6");
  char *file1 = xasprintf ("%s/file1", temp_dir);
  support_write_file_string (file1, "file1");
  add_temp_file (file1);
  FILE *fp;
  int ret;

  verbose_printf ("Testing reopening stdin\n");
  fp = FREOPEN (file1, "r", stdin);
  TEST_VERIFY_EXIT (fp == stdin);
  ret = getchar ();
  TEST_COMPARE (ret, 'f');
  ret = getchar ();
  TEST_COMPARE (ret, 'i');
  ret = getchar ();
  TEST_COMPARE (ret, 'l');
  ret = getchar ();
  TEST_COMPARE (ret, 'e');
  ret = getchar ();
  TEST_COMPARE (ret, '1');
  ret = getchar ();
  TEST_COMPARE (ret, EOF);
  xfclose (fp);

  verbose_printf ("Testing reopening stderr\n");
  fp = FREOPEN (file1, "w+", stderr);
  TEST_VERIFY_EXIT (fp == stderr);
  errno = EINVAL;
  perror ("test");
  ret = fseek (fp, 0, SEEK_SET);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE_FILE_STRING (fp, "test: Invalid argument\n");
  xfclose (fp);

  verbose_printf ("Testing reopening stdout\n");
  /* Defer checks until the old stdout has been restored to make it
     more likely any errors are written to the old stdout (rather than
     the temporary file used for the redirected stdout).  */
  int old_stdout = dup (STDOUT_FILENO);
  TEST_VERIFY_EXIT (old_stdout != -1);
  int ret_fseek = 0;
  int ret_compare = 0;
  fp = FREOPEN (file1, "w+", stdout);
  int fp_eq_stdout = fp == stdout;
  if (fp != NULL)
    {
      printf ("reopened\n");
      ret_fseek = fseek (fp, 0, SEEK_SET);
      ret_compare = support_compare_file_string (fp, "reopened\n");
    }
  xfclose (fp);
  stdout = fdopen (old_stdout, "w");
  TEST_VERIFY (fp_eq_stdout);
  TEST_COMPARE (ret_fseek, 0);
  TEST_COMPARE (ret_compare, 0);
  xfclose (stdout);

  free (temp_dir);
  free (file1);
  return 0;
}

#include <support/test-driver.c>
