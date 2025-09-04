/* Test changing the protections of a file relative to an open directory.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.

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

#include <config.h>

#include <sys/stat.h>

#include "signature.h"
SIGNATURE_CHECK (fchmodat, int, (int, const char *, mode_t, int));

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "macros.h"

#define BASE "test-fchmodat."

int
main (void)
{
  /* Test behaviour for invalid file descriptors.  */
  {
    errno = 0;
    ASSERT (fchmodat (-1, "foo", 0600, 0) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (fchmodat (99, "foo", 0600, 0) == -1);
    ASSERT (errno == EBADF);
  }

  /* Test that fchmodat works on regular files.  */
  {
    struct stat statbuf;

    unlink (BASE "file");
    ASSERT (close (creat (BASE "file", 0600)) == 0);
    ASSERT (fchmodat (AT_FDCWD, BASE "file", 0400, 0) == 0);
    ASSERT (stat (BASE "file", &statbuf) >= 0);
    ASSERT ((statbuf.st_mode & 0700) == 0400);

    errno = 0;
    ASSERT (fchmodat (AT_FDCWD, BASE "file/", 0600, 0) == -1);
    ASSERT (errno == ENOTDIR);

    /* Clean up.  */
    ASSERT (chmod (BASE "file", 0600) == 0);
    ASSERT (unlink (BASE "file") == 0);
  }

  /* Test that fchmodat works on directories.  */
  {
    struct stat statbuf;
    rmdir (BASE "dir");
    ASSERT (mkdir (BASE "dir", 0700) == 0);
    ASSERT (fchmodat (AT_FDCWD, BASE "dir", 0500, 0) == 0);
    ASSERT (stat (BASE "dir", &statbuf) >= 0);
    ASSERT ((statbuf.st_mode & 0700) == 0500);
    ASSERT (fchmodat (AT_FDCWD, BASE "dir/", 0700, 0) == 0);

    /* Clean up.  */
    ASSERT (rmdir (BASE "dir") == 0);
  }

  /* Test that fchmodat works on non-symlinks, when given
     the AT_SYMLINK_NOFOLLOW flag.  */
  {
    struct stat statbuf;
    unlink (BASE "file");
    ASSERT (close (creat (BASE "file", 0600)) == 0);
    ASSERT (fchmodat (AT_FDCWD, BASE "file", 0400, AT_SYMLINK_NOFOLLOW) == 0);
    ASSERT (stat (BASE "file", &statbuf) >= 0);
    ASSERT ((statbuf.st_mode & 0700) == 0400);
    /* Clean up.  */
    ASSERT (chmod (BASE "file", 0600) == 0);
    ASSERT (unlink (BASE "file") == 0);
  }

  return 0;
}
