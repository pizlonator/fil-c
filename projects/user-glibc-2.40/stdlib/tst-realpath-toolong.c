/* Verify that realpath returns NULL with ENAMETOOLONG if the result exceeds
   NAME_MAX.
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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BASENAME "tst-realpath-toolong."

int
do_test (void)
{
#ifndef PATH_MAX
  return EXIT_UNSUPPORTED;
#else
  char *base = support_create_and_chdir_toolong_temp_directory (BASENAME);

  char buf[PATH_MAX + 1];
  const char *res = realpath (".", buf);

  /* canonicalize.c states that if the real path is >= PATH_MAX, then
     realpath returns NULL and sets ENAMETOOLONG.  */
  TEST_VERIFY (res == NULL);
  TEST_VERIFY (errno == ENAMETOOLONG);

  free (base);
  return 0;
#endif
}

#include <support/test-driver.c>
