/* Test assert's compliance with POSIX requirements.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

/* This test verifies that a failed assertion acts in accordance with
   the POSIX requirements, despite any internal failures.  We do so by
   calling test routines via fork() and monitoring their exit codes
   and stderr, while possibly forcing internal functions (malloc) to
   fail.  */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#undef NDEBUG
#include <assert.h>

#include <support/check.h>
#include <support/support.h>
#include <support/capture_subprocess.h>
#include <support/xstdio.h>

/* We need to be able to make malloc() "fail" so that __asprintf
   fails.  */

void * (*next_malloc)(size_t sz) = 0;
static volatile bool fail_malloc = 0;

void *
malloc (size_t sz)
{
  if (fail_malloc)
    return NULL;
  if (next_malloc == 0)
    next_malloc = dlsym (RTLD_NEXT, "malloc");
  if (next_malloc == 0)
    return NULL;
  return next_malloc (sz);
}

/* We can tell if abort() is called by looking for the custom return
   value.  */

void
abort_handler(int s)
{
  _exit(5);
}

static int do_lineno;
static const char *do_funcname;

/* Hack to get the line of the assert.  */
#define GET_INFO_1(l)	      \
  if (closure != NULL)	      \
    {			      \
      do_lineno = l;	      \
      do_funcname = __func__; \
      return; \
    }
#define GET_INFO() GET_INFO_1(__LINE__+1)

/* These are the two test cases.  */

static void
test_assert_normal (void *closure)
{
  if (closure == NULL)
    signal (SIGABRT, abort_handler);

  GET_INFO ();
  assert (1 == 2);
}


static void
test_assert_nomalloc (void *closure)
{
  if (closure == NULL)
    {
      signal (SIGABRT, abort_handler);
      fail_malloc = 1;
    }

  GET_INFO ();
  assert (1 == 2);
}

static void
check_posix (const char *buf, int rv, int line,
	     const char *funcname, const char *testarg)
{
  /* POSIX requires that a failed assertion write a diagnostic to
     stderr and call abort.  The diagnostic must include the filename,
     line number, and function where the assertion failed, along with
     the text of the assert() argument.
  */
  char linestr[100];

  sprintf (linestr, "%d", line);

  /* If abort is called, our handler will return 5.  */
  TEST_VERIFY (rv == 5);

  TEST_VERIFY (strstr (buf, __FILE__) != NULL);
  TEST_VERIFY (strstr (buf, linestr) != NULL);
  TEST_VERIFY (strstr (buf, funcname) != NULL);
  TEST_VERIFY (strstr (buf, testarg) != NULL);

}

static void
one_test (void (*func)(void *))
{
  struct support_capture_subprocess sp;
  int rv;

  func (&do_lineno);
  printf("running test for %s, line %d\n", do_funcname, do_lineno);

  sp = support_capture_subprocess (func, NULL);
  rv = WEXITSTATUS (sp.status);

  check_posix (sp.err.buffer, rv, do_lineno, do_funcname, "1 == 2");

  TEST_VERIFY (strstr (sp.err.buffer, "failed.\n") != NULL);

  support_capture_subprocess_free (&sp);
}

static int
do_test(void)
{
  one_test (test_assert_normal);
  one_test (test_assert_nomalloc);

  return 0;
}

#include <support/test-driver.c>
