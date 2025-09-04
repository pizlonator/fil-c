/* Testcase for BZ 30932.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>

jmp_buf chk_fail_buf;
bool chk_fail_ok;

const char *str2 = "F";
char buf2[10] = "%s";

static int
do_test (void)
{
  struct rlimit rl;
  int max_fd = 24;

  if (getrlimit (RLIMIT_NOFILE, &rl) == -1)
    FAIL_EXIT1 ("getrlimit (RLIMIT_NOFILE): %m");

  max_fd = (rl.rlim_cur < max_fd ? rl.rlim_cur : max_fd);
  rl.rlim_cur = max_fd;

  if (setrlimit (RLIMIT_NOFILE, &rl) == 1)
    FAIL_EXIT1 ("setrlimit (RLIMIT_NOFILE): %m");

  /* Exhaust the file descriptor limit with temporary files.  */
  int nfiles = 0;
  for (; nfiles < max_fd; nfiles++)
    {
      int fd = create_temp_file ("tst-sprintf-fortify-rdonly-.", NULL);
      if (fd == -1)
	{
	  if (errno != EMFILE)
	    FAIL_EXIT1 ("create_temp_file: %m");
	  break;
	}
    }
  TEST_VERIFY_EXIT (nfiles != 0);

  /* When the format string is writable and contains %n,
     with -D_FORTIFY_SOURCE=2 it causes __chk_fail.  However, if libc can not
     open procfs to check if the input format string in within a writable
     memory segment, the fortify version can not perform the check.  */
  char buf[128];
  int n1;
  int n2;

  strcpy (buf2 + 2, "%n%s%n");
  if (sprintf (buf, buf2, str2, &n1, str2, &n2) != 2
      || n1 != 1 || n2 != 2)
    FAIL_EXIT1 ("sprintf failed: %s %d %d", buf, n1, n2);

  return 0;
}

#include <support/test-driver.c>
