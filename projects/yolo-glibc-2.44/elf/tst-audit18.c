/* Check DT_AUDIT with dlmopen.
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

#include <array_length.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <gnu/lib-names.h>
#include <support/capture_subprocess.h>
#include <support/check.h>
#include <support/xdlfcn.h>
#include <support/xstdio.h>
#include <support/support.h>

static int restart;
#define CMDLINE_OPTIONS \
  { "restart", no_argument, &restart, 1 },

static int
handle_restart (void)
{
  {
    void *h = xdlmopen (LM_ID_NEWLM, LIBC_SO, RTLD_NOW);

    pid_t (*s) (void) = xdlsym (h, "getpid");
    TEST_COMPARE (s (), getpid ());

    xdlclose (h);
  }

  {
    void *h = xdlmopen (LM_ID_NEWLM, "tst-audit18mod.so", RTLD_NOW);

    int (*foo) (void) = xdlsym (h, "foo");
    TEST_COMPARE (foo (), 10);

    xdlclose (h);
  }

  return 0;
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

  setenv ("LD_AUDIT", "tst-auditmod18.so", 0);
  struct support_capture_subprocess result
    = support_capture_subprogram (spargv[0], spargv, NULL);
  support_capture_subprocess_check (&result, "tst-audit18", 0, sc_allow_stderr);

  struct
  {
    const char *name;
    bool found;
  } audit_iface[] =
  {
    { "la_version", false },
    { "la_objsearch", false },
    { "la_activity", false },
    { "la_objopen", false },
    { "la_objclose", false },
    { "la_preinit", false },
#if __WORDSIZE == 32
    { "la_symbind32", false },
#elif __WORDSIZE == 64
    { "la_symbind64", false },
#endif
  };

  /* Some hooks are called more than once but the test only check if any
     is called at least once.  */
  FILE *out = xfmemopen (result.err.buffer, result.err.length, "r");
  char *buffer = NULL;
  size_t buffer_length = 0;
  while (xgetline (&buffer, &buffer_length, out))
    {
      for (int i = 0; i < array_length (audit_iface); i++)
	if (strncmp (buffer, audit_iface[i].name,
		     strlen (audit_iface[i].name)) == 0)
	  audit_iface[i].found = true;
    }
  free (buffer);
  xfclose (out);

  for (int i = 0; i < array_length (audit_iface); i++)
    TEST_COMPARE (audit_iface[i].found, true);

  support_capture_subprocess_free (&result);

  return 0;
}

#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
