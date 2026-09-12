/* Check DTAUDIT and vDSO interaction.
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

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

static uintptr_t vdso_addr;

static int
handle_restart (void)
{
  fprintf (stderr, "vdso: %p\n", (void*) vdso_addr);
  return 0;
}

static uintptr_t
parse_address (const char *str)
{
  void *r;
  TEST_COMPARE (sscanf (str, "%p\n", &r), 1);
  return (uintptr_t) r;
}

static inline bool
startswith (const char *str, const char *pre)
{
  size_t lenpre = strlen (pre);
  size_t lenstr = strlen (str);
  return lenstr >= lenpre && memcmp (pre, str, lenpre) == 0;
}

static int
do_test (int argc, char *argv[])
{
  vdso_addr = getauxval (AT_SYSINFO_EHDR);
  if (vdso_addr == 0)
    FAIL_UNSUPPORTED ("getauxval (AT_SYSINFO_EHDR) returned 0");

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

  setenv ("LD_AUDIT", "tst-auditmod22.so", 0);
  struct support_capture_subprocess result
    = support_capture_subprogram (spargv[0], spargv, NULL);
  support_capture_subprocess_check (&result, "tst-audit22", 0, sc_allow_stderr);

  /* The respawned process should always print the vDSO address (otherwise it
     will fails as unsupported).  However, on some architectures the audit
     module might see the vDSO with l_addr being 0, meaning a fixed mapping
     (linux-gate.so).  In this case we don't check its value against
     AT_SYSINFO_EHDR one.  */
  uintptr_t vdso_process = 0;
  bool vdso_audit_found = false;
  uintptr_t vdso_audit = 0;

  FILE *out = xfmemopen (result.err.buffer, result.err.length, "r");
  char *buffer = NULL;
  size_t buffer_length = 0;
  while (xgetline (&buffer, &buffer_length, out))
    {
      if (startswith (buffer, "vdso: "))
	vdso_process = parse_address (buffer + strlen ("vdso: "));
      else if (startswith (buffer, "vdso found: "))
	{
	  vdso_audit = parse_address (buffer + strlen ("vdso found: "));
          vdso_audit_found = true;
	}
    }

  TEST_COMPARE (vdso_audit_found, true);
  if (vdso_audit != 0)
    TEST_COMPARE (vdso_process, vdso_audit);

  free (buffer);
  xfclose (out);

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
