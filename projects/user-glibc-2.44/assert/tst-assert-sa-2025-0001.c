/* Test for CVE-2025-0395.
   Copyright The GNU Toolchain Authors.
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

/* Test that a large enough __progname does not result in a buffer overflow
   when printing an assertion failure.  This was CVE-2025-0395.  */
#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xstdio.h>
#include <support/xunistd.h>

extern const char *__progname;

int
do_test (int argc, char **argv)
{

  support_need_proc ("Reads /proc/self/maps to add guards to writable maps.");
  ignore_stderr ();

  /* XXX assumes that the assert is on a 2 digit line number.  */
  const char *prompt = ": %s:99: do_test: Assertion `argc < 1' failed.\n";

  int ret = fprintf (stderr, prompt, __FILE__);
  if (ret < 0)
    FAIL_EXIT1 ("fprintf failed: %m\n");

  size_t pagesize = getpagesize ();
  size_t namesize = pagesize - 1 - ret;

  /* Alter the progname so that the assert message fills the entire page.  */
  char progname[namesize];
  memset (progname, 'A', namesize - 1);
  progname[namesize - 1] = '\0';
  __progname = progname;

  FILE *f = xfopen ("/proc/self/maps", "r");
  char *line = NULL;
  size_t len = 0;
  uintptr_t prev_to = 0;

  /* Pad the beginning of every writable mapping with a PROT_NONE map.  This
     ensures that the mmap in the assert_fail path never ends up below a
     writable map and will terminate immediately in case of a buffer
     overflow.  */
  while (xgetline (&line, &len, f))
    {
      uintptr_t from, to;
      char perm[4];

      sscanf (line, "%" SCNxPTR "-%" SCNxPTR " %c%c%c%c ",
	      &from, &to,
	      &perm[0], &perm[1], &perm[2], &perm[3]);

      bool writable = (memchr (perm, 'w', 4) != NULL);

      if (prev_to != 0 && from - prev_to > pagesize && writable)
	xmmap ((void *) from - pagesize, pagesize, PROT_NONE,
	       MAP_ANONYMOUS | MAP_PRIVATE, 0);

      prev_to = to;
    }

  xfclose (f);

  assert (argc < 1);
  return 0;
}

#define EXPECTED_SIGNAL SIGABRT
#define TEST_FUNCTION_ARGV do_test
#include <support/test-driver.c>
