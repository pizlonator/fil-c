/* Check LD_AUDIT and LD_BIND_NOW.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/xstdio.h>
#include <support/support.h>
#include <sys/auxv.h>

#include "tst-audit25.h"

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

void tst_audit25mod1_func1 (void);
void tst_audit25mod1_func2 (void);
void tst_audit25mod2_func1 (void);
void tst_audit25mod2_func2 (void);

static int
handle_restart (void)
{
  tst_audit25mod1_func1 ();
  tst_audit25mod1_func2 ();
  tst_audit25mod2_func1 ();
  tst_audit25mod2_func2 ();

  return 0;
}

static int
do_test (int argc, char *argv[])
{
  /* We must have either:
     - One or four parameters left if called initially:
       + path to ld.so         optional
       + "--library-path"      optional
       + the library path      optional
       + the application name  */

  if (restart)
    return handle_restart ();

  setenv ("LD_AUDIT", "tst-auditmod25.so", 0);

  char *spargv[9];
  int i = 0;
  for (; i < argc - 1; i++)
    spargv[i] = argv[i + 1];
  spargv[i++] = (char *) "--direct";
  spargv[i++] = (char *) "--restart";
  spargv[i] = NULL;

  {
    struct support_capture_subprocess result
      = support_capture_subprogram (spargv[0], spargv, NULL);
    support_capture_subprocess_check (&result, "tst-audit25a", 0,
				      sc_allow_stderr);

    /* tst-audit25a and tst-audit25mod1 are built with -Wl,-z,now, but
       tst-audit25mod2 is built with -Wl,-z,lazy.  So only
       tst_audit25mod4_func1 (called by tst_audit25mod2_func1) should not
       have LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT.  */
    const char *expected[] =
      {
	"la_symbind: tst_audit25mod3_func1 1\n",
	"la_symbind: tst_audit25mod1_func1 1\n",
	"la_symbind: tst_audit25mod2_func1 1\n",
	"la_symbind: tst_audit25mod1_func2 1\n",
	"la_symbind: tst_audit25mod2_func2 1\n",
	"la_symbind: tst_audit25mod4_func1 0\n",
      };
    compare_output (result.err.buffer, result.err.length,
		    expected, array_length(expected));

    support_capture_subprocess_free (&result);
  }

  {
    setenv ("LD_BIND_NOW", "1", 0);
    struct support_capture_subprocess result
      = support_capture_subprogram (spargv[0], spargv, NULL);
    support_capture_subprocess_check (&result, "tst-audit25a", 0,
				      sc_allow_stderr);

    /* With LD_BIND_NOW all symbols are expected to have
       LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT.  Also the resolution
       order is done in breadth-first order.  */
    const char *expected[] =
      {
	"la_symbind: tst_audit25mod4_func1 1\n",
	"la_symbind: tst_audit25mod3_func1 1\n",
	"la_symbind: tst_audit25mod1_func1 1\n",
	"la_symbind: tst_audit25mod2_func1 1\n",
	"la_symbind: tst_audit25mod1_func2 1\n",
	"la_symbind: tst_audit25mod2_func2 1\n",
      };
    compare_output (result.err.buffer, result.err.length,
		    expected, array_length(expected));

    support_capture_subprocess_free (&result);
  }

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
