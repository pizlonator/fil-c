/* Test freopen in chroot.
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

#include <mcheck.h>
#include <stdio.h>
#include <stdlib.h>

#include <support/check.h>
#include <support/file_contents.h>
#include <support/namespace.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

int
do_test (void)
{
  mtrace ();
  char *temp_dir;
  FILE *fp;
  int ret;

  /* These chroot tests verify that either reopening a renamed or
     deleted file works even in the absence of /proc, or that it fails
     (without memory leaks); thus, for example, such reopening does
     not crash in the absence of /proc.  */

  support_become_root ();
  if (!support_can_chroot ())
    return EXIT_UNSUPPORTED;

  temp_dir = support_create_temp_directory ("tst-freopen4");
  xchroot (temp_dir);

  /* Test freopen with NULL, renamed file.  This verifies that
     reopening succeeds (and resets the file position indicator to
     start of file) even when the original path could no longer be
     opened, or fails without a memory leak.  (It is not possible to
     use <support/descriptors.h> to test for file descriptor leaks
     here, because that also depends on /proc.)  */

  verbose_printf ("testing freopen with NULL, renamed file\n");
  fp = xfopen ("/file1", "w+");
  ret = fputs ("file has been renamed", fp);
  TEST_VERIFY (ret >= 0);
  ret = rename ("/file1", "/file1a");
  TEST_COMPARE (ret, 0);
  fp = FREOPEN (NULL, "r+", fp);
  if (fp != NULL)
    {
      puts ("freopen of renamed file succeeded");
      TEST_COMPARE_FILE_STRING (fp, "file has been renamed");
      xfclose (fp);
    }
  else
    puts ("freopen of renamed file failed (OK)");
  ret = rename ("/file1a", "/file1");
  TEST_COMPARE (ret, 0);

  /* Test freopen with NULL, deleted file.  This verifies that
     reopening succeeds (and resets the file position indicator to
     start of file) even when the original path could no longer be
     opened, or fails without a memory leak.  */

  verbose_printf ("testing freopen with NULL, deleted file\n");
  fp = xfopen ("/file1", "r+");
  ret = fputs ("file has now been deleted", fp);
  TEST_VERIFY (ret >= 0);
  ret = remove ("/file1");
  TEST_COMPARE (ret, 0);
  fp = FREOPEN (NULL, "r+", fp);
  if (fp != NULL)
    {
      puts ("freopen of deleted file succeeded");
      TEST_COMPARE_FILE_STRING (fp, "file has now been deleted");
      xfclose (fp);
    }
  else
    puts ("freopen of deleted file failed (OK)");

  free (temp_dir);
  return 0;
}

#include <support/test-driver.c>
