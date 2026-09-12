/* Check if DT_AUDIT a module with la_plt{enter,exit} call la_symbind
   for lazy resolution.
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

#include <getopt.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/xstdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

int tst_audit18bmod1_func (void);

static int
handle_restart (void)
{
  TEST_COMPARE (tst_audit18bmod1_func (), 10);
  return 0;
}

static inline bool
startswith (const char *str, const char *pre)
{
  size_t lenpre = strlen (pre);
  size_t lenstr = strlen (str);
  return lenstr < lenpre ? false : memcmp (pre, str, lenpre) == 0;
}

static int
do_test (int argc, char *argv[])
{
  /* We must have either:
     - One our fource parameters left if called initially:
       + path to ld.so         optional
       + "--library-path"      optional
       + the library path      optional
       + the application name  */

  if (restart)
    return handle_restart ();

  char *spargv[9];
  int i = 0;
  for (; i < argc - 1; i++)
    spargv[i] = argv[i + 1];
  spargv[i++] = (char *) "--direct";
  spargv[i++] = (char *) "--restart";
  spargv[i] = NULL;

  setenv ("LD_AUDIT", "tst-auditmod18b.so", 0);
  struct support_capture_subprocess result
    = support_capture_subprogram (spargv[0], spargv, NULL);
  support_capture_subprocess_check (&result, "tst-audit18b", 0, sc_allow_stderr);

  bool find_symbind = false;

  FILE *out = xfmemopen (result.err.buffer, result.err.length, "r");
  char *buffer = NULL;
  size_t buffer_length = 0;
  while (xgetline (&buffer, &buffer_length, out))
    if (startswith (buffer, "la_symbind: tst_audit18bmod1_func") == 0)
      find_symbind = true;

  TEST_COMPARE (find_symbind, true);

  free (buffer);
  xfclose (out);

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
