/* Test for proper error/errno handling in clone.
   Copyright (C) 2006-2024 Free Software Foundation, Inc.
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

/* BZ #2386, BZ #31402 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <stackinfo.h>  /* For _STACK_GROWS_{UP,DOWN}.  */
#include <support/check.h>

volatile unsigned v = 0xdeadbeef;

int child_fn(void *arg)
{
  puts ("FAIL: in child_fn(); should not be here");
  exit(1);
}

static int
__attribute__((noinline))
do_clone (int (*fn)(void *), void *stack)
{
  int result;
  unsigned int a = v;
  unsigned int b = v;
  unsigned int c = v;
  unsigned int d = v;
  unsigned int e = v;
  unsigned int f = v;
  unsigned int g = v;
  unsigned int h = v;
  unsigned int i = v;
  unsigned int j = v;
  unsigned int k = v;
  unsigned int l = v;
  unsigned int m = v;
  unsigned int n = v;
  unsigned int o = v;

  result = clone (fn, stack, 0, NULL);

  /* Check that clone does not clobber call-saved registers.  */
  TEST_VERIFY (a == v && b == v && c == v && d == v && e == v && f == v
	       && g == v && h == v && i == v && j == v && k == v && l == v
	       && m == v && n == v && o == v);

  return result;
}

static void
__attribute__((noinline))
do_test_single (int (*fn)(void *), void *stack)
{
  printf ("%s (fn=%p, stack=%p)\n", __FUNCTION__, fn, stack);
  errno = 0;

  int result = do_clone (fn, stack);

  TEST_COMPARE (errno, EINVAL);
  TEST_COMPARE (result, -1);
}

static int
do_test (void)
{
  char st[128 * 1024] __attribute__ ((aligned));
  void *stack = NULL;
#if _STACK_GROWS_DOWN
  stack = st + sizeof (st);
#elif _STACK_GROWS_UP
  stack = st;
#else
# error "Define either _STACK_GROWS_DOWN or _STACK_GROWS_UP"
#endif

  do_test_single (child_fn, NULL);
  do_test_single (NULL, stack);
  do_test_single (NULL, NULL);

  return 0;
}

#include <support/test-driver.c>
