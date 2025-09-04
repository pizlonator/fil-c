/* Tests of areadlinkat_with_size.
   Copyright (C) 2009-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Eric Blake <ebb9@byu.net>, 2009.  */

#include <config.h>

#include "areadlink.h"

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-areadlinkat-with-size.t"

#include "test-areadlink.h"

static int dfd = AT_FDCWD;

/* Wrapper for testing areadlinkat_with_size.  */
static char *
do_areadlinkat_with_size (char const *name, size_t size)
{
  return areadlinkat_with_size (dfd, name, size);
}

int
main (void)
{
  int result;

  /* Remove any leftovers from a previous partial run.  */
  ignore_value (system ("rm -rf " BASE "*"));

  /* Basic tests.  */
  result = test_areadlink (do_areadlinkat_with_size, false);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_areadlink (do_areadlinkat_with_size, false) == result);

  /* Relative tests.  */
  if (result == 77)
    fputs ("skipping test: symlinks not supported on this file system\n",
           stderr);
  else
    {
      char *buf;
      ASSERT (symlink ("nowhere", BASE "link") == 0);
      ASSERT (mkdir (BASE "dir", 0700) == 0);
      ASSERT (chdir (BASE "dir") == 0);
      buf = areadlinkat_with_size (dfd, BASE "link", strlen (BASE "link"));
      ASSERT (buf);
      ASSERT (strcmp (buf, "nowhere") == 0);
      free (buf);
      errno = 0;
      ASSERT (areadlinkat_with_size (-1, BASE "link", 1) == NULL);
      ASSERT (errno == EBADF);
      errno = 0;
      ASSERT (areadlinkat_with_size (AT_FDCWD, BASE "link", 1) == NULL);
      ASSERT (errno == ENOENT);
      ASSERT (chdir ("..") == 0);
      ASSERT (rmdir (BASE "dir") == 0);
      ASSERT (unlink (BASE "link") == 0);
    }

  ASSERT (close (dfd) == 0);
  return result;
}
