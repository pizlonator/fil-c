/* Test pthread_mutexattr_gettype and pthread_mutexattr_settype with
   valid and invalid kinds.

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

#include <errno.h>
#include <stdio.h>
#include <pthread.h>

#include <support/check.h>
#include <support/xthread.h>


static void
do_test_kind (int test_kind, const char *name)
{
  printf ("testing %s\n", name);
  pthread_mutexattr_t attr;
  xpthread_mutexattr_init (&attr);
  int ret = pthread_mutexattr_settype (&attr, test_kind);
  TEST_COMPARE (ret, 0);
  int kind = 123456789;
  ret = pthread_mutexattr_gettype (&attr, &kind);
  TEST_COMPARE (ret, 0);
  TEST_COMPARE (kind, test_kind);
}

int
do_test (void)
{
  do_test_kind (PTHREAD_MUTEX_NORMAL, "PTHREAD_MUTEX_NORMAL");
  do_test_kind (PTHREAD_MUTEX_ERRORCHECK, "PTHREAD_MUTEX_ERRORCHECK");
  do_test_kind (PTHREAD_MUTEX_RECURSIVE, "PTHREAD_MUTEX_RECURSIVE");
  do_test_kind (PTHREAD_MUTEX_DEFAULT, "PTHREAD_MUTEX_DEFAULT");

  /* Also test an invalid kind.  */
  pthread_mutexattr_t attr;
  xpthread_mutexattr_init (&attr);
  int ret = pthread_mutexattr_settype (&attr, 123456789);
  TEST_COMPARE (ret, EINVAL);

  return 0;
}

#include <support/test-driver.c>
