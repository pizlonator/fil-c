/* Check if internal buffer reallocation work for large paths (BZ #28126)
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
#include <ftw.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xunistd.h>
#include <stdio.h>

static int
my_func (const char *file, const struct stat *sb, int flag)
{
  return 0;
}

static const char folder[NAME_MAX] = { [0 ... 253] = 'a', [254] = '\0' };

#define NSUBFOLDERS 16
static int nsubfolders;

static void
do_cleanup (void)
{
  xchdir ("..");
  for (int i = 0; i < nsubfolders; i++)
    {
      remove (folder);
      xchdir ("..");
    }
  remove (folder);
}
#define CLEANUP_HANDLER do_cleanup

static void
check_mkdir (const char *path)
{
  int r = mkdir (path, 0777);
  /* Some filesystem such as overlayfs does not support larger path required
     to trigger the internal buffer reallocation.  */
  if (r != 0)
    {
      if (errno == ENAMETOOLONG)
	FAIL_UNSUPPORTED ("the filesystem does not support the required"
			  "large path");
      else
	FAIL_EXIT1 ("mkdir (\"%s\", 0%o): %m", folder, 0777);
    }
}

static int
do_test (void)
{
  char *tempdir = support_create_temp_directory ("tst-bz28126");

  /* Create path with various subfolders to force an internal buffer
     reallocation within ntfw.  */
  char *path = xasprintf ("%s/%s", tempdir, folder);
  check_mkdir (path);
  xchdir (path);
  free (path);
  for (int i = 0; i < NSUBFOLDERS - 1; i++)
    {
      check_mkdir (folder);
      xchdir (folder);
      nsubfolders++;
    }

  TEST_COMPARE (ftw (tempdir, my_func, 20), 0);

  free (tempdir);

  do_cleanup ();

  return 0;
}

#include <support/test-driver.c>
