/* Test various fchmod error cases.
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
#include <stdio.h>
#include <support/check.h>
#include <support/xunistd.h>
#include <unistd.h>

static int
do_test (void)
{
  {
    /* Permissions on /dev/null (the opened descriptor) cannot be changed.  */
    int fd = xopen ("/dev/null", O_RDWR, 0);
    if (getuid () == 0)
      puts ("info: /dev/null fchmod test skipped because of root privileges");
    else
      {
        errno = 0;
        TEST_COMPARE (fchmod (fd, 0), -1);
        TEST_COMPARE (errno, EPERM);
      }
    xclose (fd);

    /* Now testing an invalid file descriptor.   */
    errno = 0;
    TEST_COMPARE (fchmod (fd, 0600), -1);
    TEST_COMPARE (errno, EBADF);
  }

  errno = 0;
  TEST_COMPARE (fchmod (-1, 0600), -1);
  TEST_COMPARE (errno, EBADF);

  errno = 0;
  TEST_COMPARE (fchmod (AT_FDCWD, 0600), -1);
  TEST_COMPARE (errno, EBADF);

  /* Linux supports fchmod on pretty much all file descriptors, so
     there is no check for failure on specific types of descriptors
     here.  */

  return 0;
}

#include <support/test-driver.c>
