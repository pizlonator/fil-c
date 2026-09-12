/* Skeleton for BTI tests with LD_AUDIT.
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

#include <getopt.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <unistd.h>

#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/xstdio.h>

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

/* Defined in tst-bti-mod.c file.  */
extern int fun (void);
typedef int (*fun_t) (void);

static int
handle_restart (void)
{
  HANDLE_RESTART;
  return 0;
}

static int
do_test (int argc, char *argv[])
{
  unsigned long hwcap2 = getauxval (AT_HWCAP2);
  if ((hwcap2 & HWCAP2_BTI) == 0)
    FAIL_UNSUPPORTED ("BTI is not supported by this system");

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

  setenv ("LD_AUDIT", AUDIT_MOD, 0);
  setenv ("GLIBC_TUNABLES", "glibc.cpu.aarch64_bti=1", 0);

  struct support_capture_subprocess result
    = support_capture_subprogram (spargv[0], spargv, NULL);
  support_capture_subprocess_check (&result, "tst-bti-audit", 0, ALLOW_OUTPUT);

  CHECK_OUTPUT;

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
