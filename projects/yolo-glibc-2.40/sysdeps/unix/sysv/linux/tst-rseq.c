/* Restartable Sequences single-threaded tests.
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

/* These tests validate that rseq is registered from main in an executable
   not linked against libpthread.  */

#include <support/check.h>
#include <stdio.h>
#include <sys/rseq.h>
#include <unistd.h>

#ifdef RSEQ_SIG
# include <errno.h>
# include <error.h>
# include <stdlib.h>
# include <string.h>
# include <syscall.h>
# include <sys/auxv.h>
# include <thread_pointer.h>
# include <tls.h>
# include "tst-rseq.h"

static void
do_rseq_main_test (void)
{
  struct pthread *pd = THREAD_SELF;

  TEST_VERIFY_EXIT (rseq_thread_registered ());
  TEST_COMPARE (__rseq_flags, 0);
  TEST_VERIFY ((char *) __thread_pointer () + __rseq_offset
               == (char *) &pd->rseq_area);
  /* The current implementation only supports the initial size.  */
  TEST_COMPARE (__rseq_size, 20);
}

static void
do_rseq_test (void)
{
  if (!rseq_available ())
    {
      FAIL_UNSUPPORTED ("kernel does not support rseq, skipping test");
    }
  printf ("info: __rseq_size: %u\n", __rseq_size);
  printf ("info: __rseq_offset: %td\n", __rseq_offset);
  printf ("info: __rseq_flags: %u\n", __rseq_flags);
  printf ("info: getauxval (AT_RSEQ_FEATURE_SIZE): %ld\n",
          getauxval (AT_RSEQ_FEATURE_SIZE));
  printf ("info: getauxval (AT_RSEQ_ALIGN): %ld\n", getauxval (AT_RSEQ_ALIGN));
  do_rseq_main_test ();
}
#else /* RSEQ_SIG */
static void
do_rseq_test (void)
{
  FAIL_UNSUPPORTED ("glibc does not define RSEQ_SIG, skipping test");
}
#endif /* RSEQ_SIG */

static int
do_test (void)
{
  do_rseq_test ();
  return 0;
}

#include <support/test-driver.c>
