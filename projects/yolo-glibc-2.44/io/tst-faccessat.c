/* Test for faccessat function.
   Copyright (C) 2006-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <support/check.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xdirent.h>
#include <support/xunistd.h>

static int dir_fd;

static void
prepare (int argc, char **argv)
{
  dir_fd = xopen (support_create_temp_directory ("tst-faccessat."),
		  O_RDONLY | O_DIRECTORY, 0);
}


static int
do_test (void)
{
  /* fdopendir takes over the descriptor, make a copy.  */
  int dupfd = xdup (dir_fd);
  xlseek (dupfd, 0, SEEK_SET);

  /* The directory should be empty save the . and .. files.  */
  DIR *dir = xfdopendir (dupfd);

  struct dirent64 *d;
  while ((d = xreaddir64 (dir)) != NULL)
    if (strcmp (d->d_name, ".") != 0 && strcmp (d->d_name, "..") != 0)
      FAIL_EXIT1 ("temp directory contains file \"%s\"\n", d->d_name);
  xclosedir (dir);

  /* Try to create a file.  */
  int fd = openat (dir_fd, "some-file", O_CREAT|O_RDWR|O_EXCL, 0666);
  if (fd == -1)
    {
      if (errno == ENOSYS)
	FAIL_UNSUPPORTED ("*at functions not supported");

      FAIL_EXIT1 ("file creation failed");
    }
  xwrite (fd, "hello", 5);
  puts ("file created");

  /* Before closing the file, try using this file descriptor to open
     another file.  This must fail.  */
  TEST_VERIFY_EXIT (faccessat (fd, "should-not-work", F_OK, AT_EACCESS) == -1);
  TEST_VERIFY_EXIT (errno == ENOTDIR);

  xclose (fd);

  TEST_VERIFY (faccessat (dir_fd, "some-file", F_OK, AT_EACCESS) == 0);
  TEST_VERIFY (faccessat (dir_fd, "some-file", W_OK, AT_EACCESS) == 0);

  errno = 0;
  if (faccessat (dir_fd, "some-file", X_OK, AT_EACCESS) != 0)
    TEST_COMPARE (errno, EACCES);
  else
    FAIL ("faccessat unexpectedly succeeded\n");

  if (fchmodat (dir_fd, "some-file", 0400, 0) != 0)
    FAIL_EXIT1 ("fchownat failed: %m\n");

  TEST_VERIFY (faccessat (dir_fd, "some-file", R_OK, AT_EACCESS) == 0);

  /* Write would succeed only for EUID root, otherwise this test should
     fail.  */
  errno = 0;
  TEST_VERIFY (faccessat (dir_fd, "some-file", W_OK, AT_EACCESS) == 0
	       ? (geteuid () == 0) : (errno == EACCES));

  /* Create a file descriptor which is closed again right away.  */
  int dir_fd2 = xdup (dir_fd);
  close (dir_fd2);

  /* With the file descriptor closed the next call must fail.  */
  TEST_VERIFY_EXIT (faccessat (dir_fd2, "some-file", F_OK, AT_EACCESS)
		    == -1);
  TEST_VERIFY_EXIT (errno == EBADF);

  /* Same with a non-existing file.  */
  TEST_VERIFY_EXIT (faccessat (dir_fd2, "non-existing-file", F_OK, AT_EACCESS)
		    == -1);
  TEST_VERIFY_EXIT (errno == EBADF);

  TEST_VERIFY (unlinkat (dir_fd, "some-file", 0) == 0);

  xclose (dir_fd);

  TEST_VERIFY_EXIT (faccessat (-1, "some-file", F_OK, AT_EACCESS) == -1);
  TEST_VERIFY_EXIT (errno == EBADF);

  return 0;
}
#define PREPARE prepare
#include <support/test-driver.c>
