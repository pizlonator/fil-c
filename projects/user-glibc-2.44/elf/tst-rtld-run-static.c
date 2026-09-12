/* Test running statically linked programs using ld.so.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

#include <support/check.h>
#include <support/support.h>
#include <support/capture_subprocess.h>
#include <string.h>
#include <stdlib.h>

static int
do_test (void)
{
  char *ldconfig_path = xasprintf ("%s/elf/ldconfig", support_objdir_root);

  {
    char *argv[] = { (char *) "ld.so", ldconfig_path, (char *) "--help", NULL };
    struct support_capture_subprocess cap
      = support_capture_subprogram (support_objdir_elf_ldso, argv, NULL);
    support_capture_subprocess_check (&cap, "no --argv0", 0, sc_allow_stdout);
    puts ("info: output without --argv0:");
    puts (cap.out.buffer);
    TEST_VERIFY (strstr (cap.out.buffer, "Usage: ldconfig [OPTION...]\n")
                 == cap.out.buffer);
    support_capture_subprocess_free (&cap);
  }

  {
    char *argv[] =
      {
        (char *) "ld.so", (char *) "--argv0", (char *) "ldconfig-argv0",
        ldconfig_path, (char *) "--help", NULL
      };
    struct support_capture_subprocess cap
      = support_capture_subprogram (support_objdir_elf_ldso, argv, NULL);
    support_capture_subprocess_check (&cap, "with --argv0", 0, sc_allow_stdout);
    puts ("info: output with --argv0:");
    puts (cap.out.buffer);
    TEST_VERIFY (strstr (cap.out.buffer, "Usage: ldconfig-argv0 [OPTION...]\n")
                 == cap.out.buffer);
    support_capture_subprocess_free (&cap);
  }

  free (ldconfig_path);
  return 0;
}

#include <support/test-driver.c>
