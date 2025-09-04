/* read smoke test for 0-sized structures.
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

/* Zero-sized structures should not result in any overflow warnings or
   errors when fortification is enabled.  */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <support/check.h>

int
do_test (void)
{
  struct test_st {} test_info[16];
  int fd = open ("/dev/zero", O_RDONLY, 0);

  if (fd == -1)
    FAIL_UNSUPPORTED ("Unable to open /dev/zero: %m");

  TEST_VERIFY_EXIT (read (fd, test_info, sizeof(test_info)) == 0);
  return 0;
}

#include <support/test-driver.c>
