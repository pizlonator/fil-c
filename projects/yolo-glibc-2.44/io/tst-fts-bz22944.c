/* Check that fts does not fail with very long paths.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#include <fts.h>
#include <errno.h>
#include <stdio.h>
#include <support/check.h>
#include <support/temp_file.h>
#include <support/xunistd.h>

#define BASENAME "tst-fts-bz22944."

enum { nested_depth = 150 };
static const char dir_name[] = { [0 ... 254] = 'A', '\0' };

static void
do_cleanup (void)
{
  xchdir ("..");
  for (int i = 0; i < nested_depth; i++)
    {
      remove (dir_name);
      xchdir ("..");
    }
  remove (dir_name);
}
#define CLEANUP_HANDLER do_cleanup

static void
check_mkdir (const char *path)
{
  int r = mkdir (path, 0700);
  /* Some filesystem such as overlayfs does not support larger path required
     to trigger the internal buffer reallocation.  */
  if (r != 0)
    {
      if (errno == ENAMETOOLONG)
	FAIL_UNSUPPORTED ("the filesystem does not support the required"
			  "large path");
      else
	FAIL_EXIT1 ("mkdir (\"%s\", 0%o): %m", path, 0700);
    }
}

int
do_test (void)
{
  char *tempdir = support_create_temp_directory (BASENAME);
  xchdir (tempdir);
  for (int i = 0; i < nested_depth; i++)
    {
      check_mkdir (dir_name);
      xchdir (dir_name);
    }

  char *paths[] = { tempdir , 0 };
  FTS *ftsp = fts_open (paths, FTS_XDEV | FTS_COMFOLLOW | FTS_PHYSICAL, 0);
  TEST_VERIFY_EXIT (ftsp != NULL);

  int num_dirs = 0;

  FTSENT *ent;
  while ((ent = fts_read(ftsp)) != 0)
    {
      switch (ent->fts_info)
	{
	case FTS_D:
	  num_dirs++;
	  break;
	default:
	  break;
	}
    }

  TEST_COMPARE (num_dirs, nested_depth + 1);
  TEST_COMPARE (errno, 0);

  fts_close (ftsp);

  do_cleanup ();

  return 0;
}

#include <support/test-driver.c>
