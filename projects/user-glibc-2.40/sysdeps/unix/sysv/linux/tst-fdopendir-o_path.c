/* Check if fdopendir fails with file descriptor opened with O_PATH (BZ 30737)
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
#include <dirent.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/xunistd.h>

static int
do_test (void)
{
  char *dirname = support_create_temp_directory ("tst-fdopendir-o_path");

  {
    int fd = xopen (dirname, O_RDONLY |  O_DIRECTORY, 0600);
    DIR *dir = fdopendir (fd);
    TEST_VERIFY_EXIT (dir != NULL);
    closedir (dir);
  }

  {
    int fd = xopen (dirname, O_RDONLY | O_PATH | O_DIRECTORY, 0600);
    TEST_VERIFY (fdopendir (fd) == NULL);
    TEST_COMPARE (errno, EBADF);
    xclose (fd);
  }

  return 0;
}

#include <support/test-driver.c>
