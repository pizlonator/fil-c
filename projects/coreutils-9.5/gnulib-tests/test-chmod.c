/* Test changing the protections of a file.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.

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
SIGNATURE_CHECK (chmod, int, (const char *, mode_t));

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "macros.h"

#define BASE "test-chmod."

int
main (void)
{
  /* Test that chmod works on regular files.  */
  {
    struct stat statbuf;
    unlink (BASE "file");
    ASSERT (close (creat (BASE "file", 0600)) == 0);
    ASSERT (chmod (BASE "file", 0400) == 0);
    ASSERT (stat (BASE "file", &statbuf) >= 0);
    ASSERT ((statbuf.st_mode & 0700) == 0400);

    errno = 0;
    ASSERT (chmod (BASE "file/", 0600) == -1);
    ASSERT (errno == ENOTDIR);

    /* Clean up.  */
    ASSERT (chmod (BASE "file", 0600) == 0);
    ASSERT (unlink (BASE "file") == 0);
  }

  /* Test that chmod works on directories.  */
  {
    struct stat statbuf;

    rmdir (BASE "dir");
    ASSERT (mkdir (BASE "dir", 0700) == 0);
    ASSERT (chmod (BASE "dir", 0500) == 0);
    ASSERT (stat (BASE "dir", &statbuf) >= 0);
    ASSERT ((statbuf.st_mode & 0700) == 0500);
    ASSERT (chmod (BASE "dir/", 0700) == 0);

    /* Clean up.  */
    ASSERT (rmdir (BASE "dir") == 0);
  }

  /* Test that chmod on symlinks modifies the symlink target.  */
  {
    unlink (BASE "file");
    unlink (BASE "link");
    if (symlink (BASE "file", BASE "link") == 0)
      {
        struct stat statbuf;
        ASSERT (close (creat (BASE "file", 0600)) == 0);
        chmod (BASE "link", 0400);
        ASSERT (stat (BASE "file", &statbuf) >= 0);
        ASSERT ((statbuf.st_mode & 0700) == 0400);
      }
    /* Clean up.  */
    unlink (BASE "file");
    unlink (BASE "link");
  }

  return 0;
}
