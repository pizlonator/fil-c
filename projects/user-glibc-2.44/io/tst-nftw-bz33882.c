/* Check if nested directory level does not overflow the stack (BZ #33882)
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

#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/xunistd.h>
#include <sys/resource.h>

/* Typical stack frame for a recursive function is 64–256 bytes, with a nested
   depth of 5000 would required around 640Kb of stack space.  */
enum { nested_depth = 5000 };
enum { stack_limit_kb = 512 };

/* Short name to maximize depth/path ratio.  */
static const char dir_name[] = "d";

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

static int
my_func (const char *file, const struct stat *sb, int flag, struct FTW *ftwbuf)
{
  return 0;
}

/* Set the RLIMIT_AS limit to the value in *LIMIT.  */
static void
xsetrlimit_stack (const struct rlimit *limit)
{
  if (setrlimit (RLIMIT_STACK, limit) != 0)
    FAIL_EXIT1 ("setrlimit (RLIMIT_STACK, %lu): %m",
                (unsigned long) limit->rlim_cur);
}

static int
do_test (void)
{
  xsetrlimit_stack (&(struct rlimit) { .rlim_cur = stack_limit_kb * 1024,
				       .rlim_max = stack_limit_kb * 1024 });

  char *tempdir = support_create_temp_directory ("tst-bz33882");

  xchdir (tempdir);
  for (int i = 0; i < nested_depth; i++)
    {
      check_mkdir (dir_name);
      xchdir (dir_name);
    }

  TEST_COMPARE (nftw (tempdir, my_func, 20, 0), 0);

  free (tempdir);

  do_cleanup ();

  return 0;
}

#include <support/test-driver.c>
