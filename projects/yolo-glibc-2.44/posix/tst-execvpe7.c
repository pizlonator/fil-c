/* Check execvpe does not fail when encountering long $PATH components.
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

#include <stdlib.h>
#include <libgen.h>
#include <limits.h>

#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <support/xunistd.h>

static char *fname;

static void
do_prepare (int c, char **argv)
{
  int fd;
  TEST_VERIFY_EXIT ((fd = create_temp_file ("testscript", &fname)) != -1);

  dprintf (fd, "echo foo\n");
  TEST_VERIFY_EXIT (fchmod (fd, 0700) == 0);
  xclose (fd);
}
#define PREPARE do_prepare

static void
run_execvpe (void *closure)
{
  char *argv[] = { fname, NULL };
  execvpe (basename (fname), argv, NULL);

  support_record_failure ();
}

static int
do_test (void)
{
  /* Linux 6.17 at least will give ENAMETOOLONG once any
     path component is > NAME_MAX, so ensure we skip such paths
     rather than failing.  [BZ #33626] */
  char *path = xasprintf ("%*s:%s", NAME_MAX+1, "", test_dir);
  TEST_VERIFY_EXIT (setenv ("PATH", path, 1) == 0);
  free (path);

  struct support_capture_subprocess result
    = support_capture_subprocess (run_execvpe, NULL);
  support_capture_subprocess_check (&result, "execvpe", 0, sc_allow_stdout);

  TEST_COMPARE_STRING (result.out.buffer, "foo\n");

  support_capture_subprocess_free (&result);

  return 0;
}

#include <support/test-driver.c>
