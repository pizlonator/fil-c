/* Test case for ignored LD_LIBRARY_PATH in static startug (bug 32976).
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <dlfcn.h>
#include <gnu/lib-names.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>
#include <support/test-driver.h>
#include <sys/wait.h>
#include <unistd.h>

/* This is the name of our test object.  Use a custom module for
   testing, so that this object does not get picked up from the system
   path.  */
static const char dso_name[] = "tst-dlopen-sgid-mod.so";

/* Used to mark the recursive invocation.  */
static const char magic_argument[] = "run-actual-test";

static int
do_test (void)
{
/* Pathname of the directory that receives the shared objects this
   test attempts to load.  */
  char *libdir = support_create_temp_directory ("tst-dlopen-sgid-");

  /* This is supposed to be ignored and stripped.  */
  TEST_COMPARE (setenv ("LD_LIBRARY_PATH", libdir, 1), 0);

  /* Copy of libc.so.6.  */
  {
    char *from = xasprintf ("%s/%s", support_objdir_root, LIBC_SO);
    char *to = xasprintf ("%s/%s", libdir, LIBC_SO);
    add_temp_file (to);
    support_copy_file (from, to);
    free (to);
    free (from);
  }

  /* Copy of the test object.   */
  {
    char *from = xasprintf ("%s/elf/%s", support_objdir_root, dso_name);
    char *to = xasprintf ("%s/%s", libdir, dso_name);
    add_temp_file (to);
    support_copy_file (from, to);
    free (to);
    free (from);
  }

  free (libdir);

  support_capture_subprogram_self_sgid (magic_argument);

  return 0;
}

static void
alternative_main (int argc, char **argv)
{
  if (argc == 2 && strcmp (argv[1], magic_argument) == 0)
    {
      if (getgid () == getegid ())
        /* This can happen if the file system is mounted nosuid.  */
        FAIL_UNSUPPORTED ("SGID failed: GID and EGID match (%jd)\n",
                          (intmax_t) getgid ());

      /* Should be removed due to SGID.  */
      TEST_COMPARE_STRING (getenv ("LD_LIBRARY_PATH"), NULL);

      TEST_VERIFY (dlopen (dso_name, RTLD_NOW) == NULL);
      {
        const char *message = dlerror ();
        TEST_COMPARE_STRING (message,
                             "tst-dlopen-sgid-mod.so:"
                             " cannot open shared object file:"
                             " No such file or directory");
      }

      support_record_failure_barrier ();
      exit (EXIT_SUCCESS);
    }
}

#define PREPARE alternative_main
#include <support/test-driver.c>
