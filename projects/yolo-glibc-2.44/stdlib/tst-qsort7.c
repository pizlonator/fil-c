/* Check exception handling from qsort (BZ 32058).
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
   <http://www.gnu.org/licenses/>.  */

#include <array_length.h>
#include <mcheck.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/xthread.h>
#include <unistd.h>

static pthread_barrier_t b;

static void
cl (void *arg)
{
}

static int
compar_func (const void *a1, const void *a2)
{
  xpthread_barrier_wait (&b);

  pthread_cleanup_push (cl, NULL);

  pause ();

  pthread_cleanup_pop (0);

  support_record_failure ();

  return 0;
}

static void *
tf (void *tf)
{
  /* An array larger than QSORT_STACK_SIZE to force memory allocation.  */
  int input[1024] = { 0 };
  qsort (input, array_length (input), sizeof input[0], compar_func);

  return NULL;
}

static int
do_test (void)
{
  mtrace ();

  xpthread_barrier_init (&b, NULL, 2);

  pthread_t thr = xpthread_create (NULL, tf, NULL);

  xpthread_barrier_wait (&b);

  xpthread_cancel (thr);

  {
    void *r = xpthread_join (thr);
    TEST_VERIFY (r == PTHREAD_CANCELED);
  }

  return 0;
}

#include <support/test-driver.c>
