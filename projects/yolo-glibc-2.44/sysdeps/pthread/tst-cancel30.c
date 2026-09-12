/* Check if printf like functions does not disable asynchronous cancellation
   mode (BZ#29214).

   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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
#include <support/check.h>
#include <support/xstdio.h>
#include <support/xthread.h>
#include <sys/syscall.h>
#include <unistd.h>

static pthread_barrier_t b;

static void *
tf (void *arg)
{
  int old;

  TEST_COMPARE (pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL), 0);

  TEST_COMPARE (pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, &old), 0);
  TEST_COMPARE (old, PTHREAD_CANCEL_ASYNCHRONOUS);

  /* Check if internal lock cleanup routines restore the cancellation type
     correctly.  */
  printf ("...\n");
  TEST_COMPARE (pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, &old), 0);
  TEST_COMPARE (old, PTHREAD_CANCEL_ASYNCHRONOUS);

  xpthread_barrier_wait (&b);

  /* Wait indefinitely for cancellation, which only works if asynchronous
     cancellation is enabled.  */
#ifdef SYS_ppoll_time64
  long int ret = syscall (SYS_ppoll_time64, NULL, 0, NULL, NULL);
  (void) ret;
# ifdef SYS_ppoll
  if (ret == -1 && errno == ENOSYS)
    syscall (SYS_ppoll, NULL, 0, NULL, NULL);
# endif
#else
# ifdef SYS_ppoll
  syscall (SYS_ppoll, NULL, 0, NULL, NULL);
# else
  for (;;);
# endif
#endif

  return 0;
}

static int
do_test (void)
{
  xpthread_barrier_init (&b, NULL, 2);

  pthread_t th = xpthread_create (NULL, tf, NULL);

  xpthread_barrier_wait (&b);

  xpthread_cancel (th);

  void *status = xpthread_join (th);
  TEST_VERIFY (status == PTHREAD_CANCELED);

  return 0;
}

/* There is no need to wait full TIMEOUT if asynchronous is not working.  */
#define TIMEOUT 3
#include <support/test-driver.c>
