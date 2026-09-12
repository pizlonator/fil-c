/* pthread_getcpuclockid should fail with ESRCH when the thread exits.
   Copyright the GNU Toolchain Authors.
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

/* The input thread descriptor to pthread_getcpuclockid needs to be valid when
   the function is called.  For the purposes of this test, this means that the
   thread should not be detached, have exited, but not joined.  This should be
   good enough to complete coverage for pthread_getcpuclockid alongside
   tst-clock2.  */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#include <support/check.h>
#include <support/test-driver.h>
#include <support/xthread.h>

void *
thr (void *in)
{
  return in;
}

int
do_test (void)
{
  clockid_t c;
  pthread_t t = xpthread_create (NULL, thr, NULL);

  int ret = 0;
  while ((ret = pthread_getcpuclockid (t, &c)) == 0)
    sched_yield ();

  TEST_COMPARE (ret, ESRCH);

  return 0;
}

#include <support/test-driver.c>
