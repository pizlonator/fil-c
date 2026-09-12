/* Testcase for BZ 30932.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
#include <support/xdlfcn.h>

static sigjmp_buf chk_fail_buf;
static volatile int ret;
static bool chk_fail_ok;

static void
handler (int sig)
{
  if (chk_fail_ok)
    {
      chk_fail_ok = false;
      longjmp (chk_fail_buf, 1);
    }
  else
    _exit (127);
}

#define FORTIFY_FAIL \
  do { printf ("Failure on line %d\n", __LINE__); ret = 1; } while (0)
#define CHK_FAIL_START \
  chk_fail_ok = true;				\
  if (! sigsetjmp (chk_fail_buf, 1))		\
    {
#define CHK_FAIL_END \
      chk_fail_ok = false;			\
      FORTIFY_FAIL;				\
    }

static const char *str2 = "F";
static char writeable_format[10] = "%s";
static char relro_format[10] __attribute__ ((section (".data.rel.ro"))) =
 "%s%n%s%n";

extern void init_writable (void);
extern int sprintf_writable (int *, int *);
extern int sprintf_relro (int *, int *);
extern int sprintf_writable_malloc (int *, int *);

#define str(__x) # __x
void (*init_writable_dlopen)(void);
int (*sprintf_writable_dlopen)(int *, int *);
int (*sprintf_rdonly_dlopen)(int *, int *);
int (*sprintf_writable_malloc_dlopen)(int *, int *);

static int
do_test (void)
{
  set_fortify_handler (handler);

  {
    void *h = xdlopen ("tst-sprintf-fortify-rdonly-dlopen.so", RTLD_NOW);
    init_writable_dlopen = xdlsym (h, str(init_writable));
    sprintf_writable_dlopen = xdlsym (h, str(sprintf_writable));
    sprintf_rdonly_dlopen = xdlsym (h, str(sprintf_relro));
    sprintf_writable_malloc_dlopen = xdlsym (h, str(sprintf_writable_malloc));
  }

  struct rlimit rl;
  int max_fd = 24;

  if (getrlimit (RLIMIT_NOFILE, &rl) == -1)
    FAIL_EXIT1 ("getrlimit (RLIMIT_NOFILE): %m");

  max_fd = (rl.rlim_cur < max_fd ? rl.rlim_cur : max_fd);
  rl.rlim_cur = max_fd;

  if (setrlimit (RLIMIT_NOFILE, &rl) == -1)
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

  strcpy (writeable_format + 2, "%n%s%n");
  init_writable ();
  init_writable_dlopen ();

  /* writeable_format is at a writable part of .bss segment, so libc should be
     able to check it without resorting to procfs.  */
  {
    char buf[128];
    int n1;
    int n2;
    CHK_FAIL_START
    sprintf (buf, writeable_format, str2, &n1, str2, &n2);
    CHK_FAIL_END
  }

  /* Same as before, but from an library.  */
  {
    int n1;
    int n2;
    CHK_FAIL_START
    sprintf_writable (&n1, &n2);
    CHK_FAIL_END
  }

  {
    int n1;
    int n2;
    CHK_FAIL_START
    sprintf_writable_dlopen (&n1, &n2);
    CHK_FAIL_END
  }

  /* relro_format is at a readonly part of .bss segment, so '%n' in format input
     should not trigger a fortify failure.  */
  {
    char buf[128];
    int n1;
    int n2;
    if (sprintf (buf, relro_format, str2, &n1, str2, &n2) != 2
	|| n1 != 1 || n2 != 2)
      FAIL_EXIT1 ("sprintf failed: %s %d %d", buf, n1, n2);
  }

  /* Same as before, but from an library.  */
  {
    int n1;
    int n2;
    if (sprintf_relro (&n1, &n2) != 2 || n1 != 1 || n2 != 2)
      FAIL_EXIT1 ("sprintf failed: %d %d", n1, n2);
  }

  {
    int n1;
    int n2;
    if (sprintf_rdonly_dlopen (&n1, &n2) != 2 || n1 != 1 || n2 != 2)
      FAIL_EXIT1 ("sprintf failed: %d %d", n1, n2);
  }

  /* However if the format string is placed on a writable memory not covered
     by ELF segments, libc needs to resort to procfs.  */
  {
    char buf[128];
    int n1;
    int n2;
    char *buf2_malloc = xstrdup (writeable_format);
    CHK_FAIL_START
    sprintf (buf, buf2_malloc, str2, &n1, str2, &n2);
    CHK_FAIL_END
  }

  /* Same as before, but from an library.  */
  {
    int n1;
    int n2;
    CHK_FAIL_START
    sprintf_writable_malloc (&n1, &n2);
    CHK_FAIL_END
  }

  {
    int n1;
    int n2;
    CHK_FAIL_START
    sprintf_writable_malloc_dlopen (&n1, &n2);
    CHK_FAIL_END
  }

  return ret;
}

#include <support/test-driver.c>
