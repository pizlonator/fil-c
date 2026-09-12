/* Test disabling of rseq registration via tunable.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.

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
# include <sys/auxv.h>
# include <dl-tls.h>
# include "tst-rseq.h"

/* Used to test private registration with the rseq system call because glibc
   rseq is disabled.  */
static __thread struct rseq local_rseq = {
  .cpu_id = RSEQ_CPU_ID_REGISTRATION_FAILED,
};

/* Used to check if the address of the rseq area comes before or after the tls
   blocks depending on the TLS model.  */
static __thread char tls_var __attribute__ ((tls_model ("initial-exec")));

/* Check that rseq can be registered and has not been taken by glibc.  */
static void
check_rseq_disabled (void)
{
  struct rseq *rseq_abi = (struct rseq *) ((char *) __thread_pointer () +
		           __rseq_offset);

#if TLS_TCB_AT_TP
  /* The rseq area block should come before the thread pointer and be at least
     32 bytes. */
  TEST_VERIFY (__rseq_offset <= -RSEQ_AREA_SIZE_INITIAL);

  /* The rseq area block should come before TLS variables.  */
  TEST_VERIFY ((intptr_t) rseq_abi < (intptr_t) &tls_var);
#elif TLS_DTV_AT_TP
  /* The rseq area block should come after the TCB, add the TLS block offset to
     the rseq offset to get a value relative to the TCB and test that it's
     non-negative.  */
  TEST_VERIFY (__rseq_offset + TLS_TP_OFFSET >= 0);

  /* The rseq area block should come after TLS variables.  */
  TEST_VERIFY ((intptr_t) rseq_abi > (intptr_t) &tls_var);
#else
# error "Either TLS_TCB_AT_TP or TLS_DTV_AT_TP must be defined"
#endif

  /* __rseq_flags is unused and should always be '0'.  */
  TEST_COMPARE (__rseq_flags, 0);

  /* When rseq is not registered, __rseq_size should always be '0'.  */
  TEST_COMPARE (__rseq_size, 0);

  /* When rseq is not registered, the 'cpu_id' field should be set to
     RSEQ_CPU_ID_REGISTRATION_FAILED.  */
  TEST_COMPARE ((int) rseq_abi->cpu_id, RSEQ_CPU_ID_REGISTRATION_FAILED);

  /* Test a rseq registration which should succeed since the internal
     registration is disabled.  */
  int ret = syscall (__NR_rseq, &local_rseq, RSEQ_AREA_SIZE_INITIAL, 0, RSEQ_SIG);
  if (ret == 0)
    {
      /* A successful registration should set the cpu id.  */
      TEST_VERIFY (local_rseq.cpu_id >= 0);

      /* Test we can also unregister rseq.  */
      ret = syscall (__NR_rseq, &local_rseq, RSEQ_AREA_SIZE_INITIAL,
                     RSEQ_FLAG_UNREGISTER, RSEQ_SIG);
      TEST_COMPARE (ret, 0);
    }
  else
    {
      /* Check if we failed with EINVAL which would mean an invalid rseq flags,
         a mis-aligned rseq area address or an incorrect rseq size.  */
      TEST_VERIFY (errno != EINVAL);

      /* Check if we failed with EBUSY which means an existing rseq
         registration. */
      TEST_VERIFY (errno != EBUSY);

      /* Check if we failed with EFAULT which means an invalid rseq area
         address.  */
      TEST_VERIFY (errno != EFAULT);
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
  printf ("info: __rseq_size: %u\n", __rseq_size);
  printf ("info: __rseq_offset: %td\n", __rseq_offset);
  printf ("info: __rseq_flags: %u\n", __rseq_flags);
  printf ("info: getauxval (AT_RSEQ_FEATURE_SIZE): %ld\n",
          getauxval (AT_RSEQ_FEATURE_SIZE));
  printf ("info: getauxval (AT_RSEQ_ALIGN): %ld\n", getauxval (AT_RSEQ_ALIGN));

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
