/* Test freopen failure.
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
#include <stdio.h>
#include <stdlib.h>

#include <support/check.h>
#include <support/descriptors.h>
#include <support/file_contents.h>
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
  char *temp_dir = support_create_temp_directory ("tst-freopen3");
  char *file1 = xasprintf ("%s/file1", temp_dir);
  support_write_file_string (file1, "file1");
  add_temp_file (file1);
  char *file2 = xasprintf ("%s/file2", temp_dir);
  support_write_file_string (file2, "file2");
  add_temp_file (file2);
  char *file_nodir = xasprintf ("%s/nodir/file", temp_dir);
  FILE *fp;
  int ret;
  int fd;

  START_TEST ("Testing w -> wx (file exists)\n");
  fp = xfopen (file1, "w");
  fp = FREOPEN (file2, "wx", fp);
  TEST_VERIFY (fp == NULL);
  END_TEST;

  /* Test old file is closed even when opening the new file fails.  */

  START_TEST ("testing r -> r (opening new file fails)\n");
  fp = xfopen (file1, "r");
  fd = fileno (fp);
  fp = FREOPEN (file_nodir, "r", fp);
  TEST_VERIFY (fp == NULL);
  errno = 0;
  ret = fcntl (fd, F_GETFL);
  TEST_COMPARE (ret, -1);
  TEST_COMPARE (errno, EBADF);
  END_TEST;

  free (temp_dir);
  free (file1);
  free (file2);
  free (file_nodir);
  return 0;
}

#include <support/test-driver.c>
