/* Tests of mkfifoat and mknodat.
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

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (mkfifoat, int, (int, char const *, mode_t));
SIGNATURE_CHECK (mknodat, int, (int, char const *, mode_t, dev_t));

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-mkfifoat.t"

#include "test-mkfifo.h"

typedef int (*test_func) (int, char const *, mode_t);

static int dfd = AT_FDCWD;

/* Wrapper to test mknodat like mkfifoat.  */
static int
test_mknodat (int fd, char const *name, mode_t mode)
{
  /* This is the only portable use of mknodat, per POSIX.  */
  return mknodat (fd, name, mode | S_IFIFO, 0);
}

/* Wrapper to test mkfifoat like mkfifo.  */
static int
do_mkfifoat (char const *name, mode_t mode)
{
  return mkfifoat (dfd, name, mode);
}

/* Wrapper to test mknodat like mkfifo.  */
static int
do_mknodat (char const *name, mode_t mode)
{
  return mknodat (dfd, name, mode | S_IFIFO, 0);
}

int
main (void)
{
  int i;
  test_func funcs[2] = { mkfifoat, test_mknodat };
  int result;

  /* Remove any leftovers from a previous partial run.  */
  ignore_value (system ("rm -rf " BASE "*"));

  /* Basic tests.  */
  result = test_mkfifo (do_mkfifoat, true);
  ASSERT (test_mkfifo (do_mknodat, false) == result);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (test_mkfifo (do_mkfifoat, false) == result);
  ASSERT (test_mkfifo (do_mknodat, false) == result);

  /* Test directory-relative handling of both functions.  */
  for (i = 0; i < 2; i++)
    {
      struct stat st;
      test_func func = funcs[i];

      /* Test behaviour for invalid file descriptors.  */
      {
        errno = 0;
        ASSERT (func (-1, "foo", 0600) == -1);
        ASSERT (errno == EBADF
                || errno == ENOSYS /* seen on mingw */
               );
      }
      {
        close (99);
        errno = 0;
        ASSERT (func (99, "foo", 0600) == -1);
        ASSERT (errno == EBADF
                || errno == ENOSYS /* seen on mingw */
               );
      }

      /* Create fifo while cwd is '.', then stat it from '..'.  */
      if (func (AT_FDCWD, BASE "fifo", 0600) != 0)
        ASSERT (errno == ENOSYS); /* seen on native Windows */
      else
        {
          errno = 0;
          ASSERT (func (dfd, BASE "fifo", 0600) == -1);
          ASSERT (errno == EEXIST);
          ASSERT (chdir ("..") == 0);
          errno = 0;
          ASSERT (fstatat (AT_FDCWD, BASE "fifo", &st, 0) == -1);
          ASSERT (errno == ENOENT);
          memset (&st, 0, sizeof st);
          ASSERT (fstatat (dfd, BASE "fifo", &st, 0) == 0);
          ASSERT (S_ISFIFO (st.st_mode));
          ASSERT (unlinkat (dfd, BASE "fifo", 0) == 0);
        }

      /* Create fifo while cwd is '..', then stat it from '.'.  */
      if (func (dfd, BASE "fifo", 0600) != 0)
        ASSERT (errno == ENOSYS); /* seen on native Windows */
      else
        {
          ASSERT (fchdir (dfd) == 0);
          errno = 0;
          ASSERT (func (AT_FDCWD, BASE "fifo", 0600) == -1);
          ASSERT (errno == EEXIST);
          memset (&st, 0, sizeof st);
          ASSERT (fstatat (AT_FDCWD, BASE "fifo", &st, AT_SYMLINK_NOFOLLOW)
                  == 0);
          ASSERT (S_ISFIFO (st.st_mode));
          memset (&st, 0, sizeof st);
          ASSERT (fstatat (dfd, BASE "fifo", &st, AT_SYMLINK_NOFOLLOW) == 0);
          ASSERT (S_ISFIFO (st.st_mode));
          ASSERT (unlink (BASE "fifo") == 0);
        }
    }

  ASSERT (close (dfd) == 0);

  return 0;
}
