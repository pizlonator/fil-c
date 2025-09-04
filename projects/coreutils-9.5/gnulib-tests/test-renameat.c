/* Tests of renameat.
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

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (renameat, int, (int, char const *, int, char const *));

#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "filenamecat.h"
#include "ignore-value.h"
#include "macros.h"

#define BASE "test-renameat.t"

#include "test-rename.h"

static int dfd1 = AT_FDCWD;
static int dfd2 = AT_FDCWD;

/* Wrapper to test renameat like rename.  */
static int
do_rename (char const *name1, char const *name2)
{
  return renameat (dfd1, name1, dfd2, name2);
}

int
main (void)
{
  int i;
  int dfd;
  char *cwd;
  int result;

  /* Clean up any trash from prior testsuite runs.  */
  ignore_value (system ("rm -rf " BASE "*"));

  /* Test behaviour for invalid file descriptors.  */
  {
    errno = 0;
    ASSERT (renameat (-1, "foo", AT_FDCWD, "bar") == -1);
    ASSERT (errno == EBADF);
  }
  {
    close (99);
    errno = 0;
    ASSERT (renameat (99, "foo", AT_FDCWD, "bar") == -1);
    ASSERT (errno == EBADF);
  }
  ASSERT (close (creat (BASE "oo", 0600)) == 0);
  {
    errno = 0;
    ASSERT (renameat (AT_FDCWD, BASE "oo", -1, "bar") == -1);
    ASSERT (errno == EBADF);
  }
  {
    errno = 0;
    ASSERT (renameat (AT_FDCWD, BASE "oo", 99, "bar") == -1);
    ASSERT (errno == EBADF);
  }
  ASSERT (unlink (BASE "oo") == 0);

  /* Test basic rename functionality, using current directory.  */
  result = test_rename (do_rename, false);
  dfd1 = open (".", O_RDONLY);
  ASSERT (0 <= dfd1);
  ASSERT (test_rename (do_rename, false) == result);
  dfd2 = dfd1;
  ASSERT (test_rename (do_rename, false) == result);
  dfd1 = AT_FDCWD;
  ASSERT (test_rename (do_rename, false) == result);
  ASSERT (close (dfd2) == 0);

  /* Create locations to manipulate.  */
  ASSERT (mkdir (BASE "sub1", 0700) == 0);
  ASSERT (mkdir (BASE "sub2", 0700) == 0);
  dfd = creat (BASE "00", 0600);
  ASSERT (0 <= dfd);
  ASSERT (close (dfd) == 0);
  cwd = getcwd (NULL, 0);
  ASSERT (cwd);

  dfd = open (BASE "sub1", O_RDONLY);
  ASSERT (0 <= dfd);
  ASSERT (chdir (BASE "sub2") == 0);

  /* There are 16 possible scenarios, based on whether an fd is
     AT_FDCWD or real, and whether a file is absolute or relative.

     To ensure that we test all of the code paths (rather than
     triggering early normalization optimizations), we use a loop to
     repeatedly rename a file in the parent directory, use an fd open
     on subdirectory 1, all while executing in subdirectory 2; all
     relative names are thus given with a leading "../".  Finally, the
     last scenario (two relative paths given, neither one AT_FDCWD)
     has two paths, based on whether the two fds are equivalent, so we
     do the other variant after the loop.  */
  for (i = 0; i < 16; i++)
    {
      int fd1 = (i & 8) ? dfd : AT_FDCWD;
      char *file1 = file_name_concat ((i & 4) ? ".." : cwd, BASE "xx", NULL);
      int fd2 = (i & 2) ? dfd : AT_FDCWD;
      char *file2 = file_name_concat ((i & 1) ? ".." : cwd, BASE "xx", NULL);

      ASSERT (sprintf (strchr (file1, '\0') - 2, "%02d", i) == 2);
      ASSERT (sprintf (strchr (file2, '\0') - 2, "%02d", i + 1) == 2);
      ASSERT (renameat (fd1, file1, fd2, file2) == 0);
      free (file1);
      free (file2);
    }
  dfd2 = open ("..", O_RDONLY);
  ASSERT (0 <= dfd2);
  ASSERT (renameat (dfd, "../" BASE "16", dfd2, BASE "17") == 0);
  ASSERT (close (dfd2) == 0);

  /* Now we change back to the parent directory, and set dfd to ".";
     using dfd in remaining tests will expose any bugs if emulation
     via /proc/self/fd doesn't check for empty names.  */
  ASSERT (chdir ("..") == 0);
  ASSERT (close (dfd) == 0);
  dfd = open (".", O_RDONLY);
  ASSERT (0 <= dfd);

  ASSERT (close (creat (BASE "sub2/file", 0600)) == 0);
  errno = 0;
  ASSERT (renameat (dfd, BASE "sub1", dfd, BASE "sub2") == -1);
  ASSERT (errno == EEXIST || errno == ENOTEMPTY);
  ASSERT (unlink (BASE "sub2/file") == 0);
  errno = 0;
  ASSERT (renameat (dfd, BASE "sub2", dfd, BASE "sub1/.") == -1);
  ASSERT (errno == EINVAL || errno == EISDIR || errno == EBUSY
          || errno == ENOTEMPTY || errno == EEXIST
          || errno == ENOENT /* WSL */);
  errno = 0;
  ASSERT (renameat (dfd, BASE "sub2/.", dfd, BASE "sub1") == -1);
  ASSERT (errno == EINVAL || errno == EBUSY || errno == EEXIST
          || errno == ENOENT /* WSL */);
  errno = 0;
  ASSERT (renameat (dfd, BASE "17", dfd, BASE "sub1") == -1);
  ASSERT (errno == EISDIR);
  errno = 0;
  ASSERT (renameat (dfd, BASE "nosuch", dfd, BASE "18") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (renameat (dfd, "", dfd, BASE "17") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (renameat (dfd, BASE "17", dfd, "") == -1);
  ASSERT (errno == ENOENT);
  errno = 0;
  ASSERT (renameat (dfd, BASE "sub2", dfd, BASE "17") == -1);
  ASSERT (errno == ENOTDIR);
  errno = 0;
  ASSERT (renameat (dfd, BASE "17/", dfd, BASE "18") == -1);
  ASSERT (errno == ENOTDIR);
  errno = 0;
  ASSERT (renameat (dfd, BASE "17", dfd, BASE "18/") == -1);
  ASSERT (errno == ENOTDIR || errno == ENOENT);

  /* Finally, make sure we can overwrite existing files.  */
  ASSERT (close (creat (BASE "sub2/file", 0600)) == 0);
  errno = 0;
  ASSERT (renameat (dfd, BASE "sub2", dfd, BASE "sub1") == 0);
  ASSERT (renameat (dfd, BASE "sub1/file", dfd, BASE "17") == 0);

  /* Cleanup.  */
  ASSERT (close (dfd) == 0);
  errno = 0;
  ASSERT (unlink (BASE "sub1/file") == -1);
  ASSERT (errno == ENOENT);
  ASSERT (unlink (BASE "17") == 0);
  ASSERT (rmdir (BASE "sub1") == 0);
  errno = 0;
  ASSERT (rmdir (BASE "sub2") == -1);
  ASSERT (errno == ENOENT);
  free (cwd);

  if (result)
    fputs ("skipping test: symlinks not supported on this file system\n",
           stderr);
  return result;
}
