/* Test disabling of rseq registration via tunable.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.

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
#include <support/check.h>
#include <support/namespace.h>
#include <support/xthread.h>
#include <sysdep.h>
#include <thread_pointer.h>
#include <sys/rseq.h>
#include <unistd.h>

#ifdef RSEQ_SIG

/* Check that rseq can be registered and has not been taken by glibc.  */
static void
check_rseq_disabled (void)
{
  struct pthread *pd = THREAD_SELF;

  TEST_COMPARE (__rseq_flags, 0);
  TEST_VERIFY ((char *) __thread_pointer () + __rseq_offset
               == (char *) &pd->rseq_area);
  TEST_COMPARE (__rseq_size, 0);
  TEST_COMPARE ((int) pd->rseq_area.cpu_id, RSEQ_CPU_ID_REGISTRATION_FAILED);

  int ret = syscall (__NR_rseq, &pd->rseq_area, sizeof (pd->rseq_area),
                     0, RSEQ_SIG);
  if (ret == 0)
    {
      ret = syscall (__NR_rseq, &pd->rseq_area, sizeof (pd->rseq_area),
                     RSEQ_FLAG_UNREGISTER, RSEQ_SIG);
      TEST_COMPARE (ret, 0);
      pd->rseq_area.cpu_id = RSEQ_CPU_ID_REGISTRATION_FAILED;
    }
  else
    {
      TEST_VERIFY (errno != -EINVAL);
      TEST_VERIFY (errno != -EBUSY);
    }
}

static void *
thread_func (void *ignored)
{
  check_rseq_disabled ();
  return NULL;
}

static void
proc_func (void *ignored)
{
  check_rseq_disabled ();
}

static int
do_test (void)
{
  puts ("info: checking main thread");
  check_rseq_disabled ();

  puts ("info: checking main thread (2)");
  check_rseq_disabled ();

  puts ("info: checking new thread");
  xpthread_join (xpthread_create (NULL, thread_func, NULL));

  puts ("info: checking subprocess");
  support_isolate_in_subprocess (proc_func, NULL);

  return 0;
}
#else /* !RSEQ_SIG */
static int
do_test (void)
{
  FAIL_UNSUPPORTED ("glibc does not define RSEQ_SIG, skipping test");
}
#endif

#include <support/test-driver.c>
