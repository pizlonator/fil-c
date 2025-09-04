/* Test checking user's permissions for a file.
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

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (faccessat, int, (int, const char *, int, int));

#include <errno.h>
#include <fcntl.h>

#include "macros.h"

#define BASE "test-faccessat.t"

int
main (void)
{
  /* Test behaviour for invalid file descriptors.  */
  {
    errno = 0;
    ASSERT (faccessat (-1, "foo", F_OK, 0) == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (faccessat (99, "foo", F_OK, 0) == -1);
    ASSERT (errno == EBADF);
  }

  /* Test behavior with trailing slash.  */

  unlink (BASE "file");
  ASSERT (faccessat (AT_FDCWD, ".", X_OK, 0) == 0);
  ASSERT (faccessat (AT_FDCWD, "./", X_OK, 0) == 0);
  ASSERT (close (open (BASE "file", O_CREAT | O_WRONLY, 0)) == 0);
  ASSERT (faccessat (AT_FDCWD, BASE "file", F_OK, 0) == 0);

  ASSERT (faccessat (AT_FDCWD, BASE "file/", F_OK, 0) != 0);
  ASSERT (errno == ENOTDIR);
  ASSERT (faccessat (AT_FDCWD, BASE "file/", R_OK, 0) != 0);
  ASSERT (errno == ENOTDIR);
  ASSERT (faccessat (AT_FDCWD, BASE "file/", W_OK, 0) != 0);
  ASSERT (errno == ENOTDIR);
  ASSERT (faccessat (AT_FDCWD, BASE "file/", X_OK, 0) != 0);
  ASSERT (errno == ENOTDIR);

  unlink (BASE "link1");
  if (symlink (".", BASE "link1") == 0)
    {
      ASSERT (faccessat (AT_FDCWD, BASE "link1", X_OK, 0) == 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link1/", X_OK, 0) == 0);

      unlink (BASE "link2");
      ASSERT (symlink (BASE "file", BASE "link2") == 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link2", F_OK, 0) == 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link2/", F_OK, 0) != 0);
      ASSERT (errno == ENOTDIR);
      ASSERT (faccessat (AT_FDCWD, BASE "link2/", R_OK, 0) != 0);
      ASSERT (errno == ENOTDIR || errno == EACCES);
      ASSERT (faccessat (AT_FDCWD, BASE "link2/", W_OK, 0) != 0);
      ASSERT (errno == ENOTDIR || errno == EACCES);
      ASSERT (faccessat (AT_FDCWD, BASE "link2/", X_OK, 0) != 0);
      ASSERT (errno == ENOTDIR || errno == EACCES);
      unlink (BASE "link2");

      unlink (BASE "link3");
      ASSERT (symlink (BASE "no-such-file", BASE "link3") == 0);
      ASSERT (faccessat (AT_FDCWD, BASE "link3", F_OK, 0) != 0);
      ASSERT (errno == ENOENT);
      ASSERT (faccessat (AT_FDCWD, BASE "link3/", F_OK, 0) != 0);
      ASSERT (errno == ENOENT);
      ASSERT (faccessat (AT_FDCWD, BASE "link3/", R_OK, 0) != 0);
      ASSERT (errno == ENOENT);
      ASSERT (faccessat (AT_FDCWD, BASE "link3/", W_OK, 0) != 0);
      ASSERT (errno == ENOENT);
      ASSERT (faccessat (AT_FDCWD, BASE "link3/", X_OK, 0) != 0);
      ASSERT (errno == ENOENT);
      unlink (BASE "link3");
    }
  unlink (BASE "link1");
  unlink (BASE "file");

  return 0;
}
