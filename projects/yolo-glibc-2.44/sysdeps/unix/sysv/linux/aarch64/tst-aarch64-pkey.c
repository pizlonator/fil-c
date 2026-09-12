/* AArch64 tests for memory protection keys.
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
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <support/xsignal.h>
#include <support/xunistd.h>
#include <sys/mman.h>
#include <array_length.h>

static sig_atomic_t sigusr1_handler_ran;
static int pkey;

/* On AArch64 access is revoked during signal handling for
  pkey > 0 because POR is reset to the default value 0x7.  */
static void
sigusr1_handler (int signum)
{
  TEST_COMPARE (signum, SIGUSR1);
  TEST_COMPARE (pkey_get (pkey) & PKEY_DISABLE_ACCESS, PKEY_DISABLE_ACCESS);
  TEST_COMPARE (pkey_get (pkey) & PKEY_DISABLE_READ, PKEY_DISABLE_READ);
  TEST_COMPARE (pkey_get (pkey) & PKEY_DISABLE_WRITE, PKEY_DISABLE_WRITE);
  TEST_COMPARE (pkey_get (pkey) & PKEY_DISABLE_EXECUTE, PKEY_DISABLE_EXECUTE);
  sigusr1_handler_ran += 1;
}

static int
do_test (void)
{
  pkey = pkey_alloc (0, PKEY_UNRESTRICTED);
  if (pkey < 0)
    {
      if (errno == ENOSYS || errno == EINVAL)
        FAIL_UNSUPPORTED
          ("kernel or CPU does not support memory protection keys");
      if (errno == ENOSPC)
        FAIL_UNSUPPORTED
          ("no keys available or kernel does not support memory"
           " protection keys");
      FAIL_EXIT1 ("pkey_alloc: %m");
    }

  long int pagesize = xsysconf (_SC_PAGESIZE);

  int *page = xmmap (NULL, pagesize, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE,
    -1);

  /* On AArch64 pkey == 0 is reserved and should never be allocated.  */
  TEST_VERIFY (pkey > 0);
  TEST_COMPARE (pkey_get(pkey), PKEY_UNRESTRICTED);

  /* Check that access is revoked during signal handling
     with initial rights being set to no restrictions.  */
  TEST_COMPARE (pkey_mprotect ((void *) page, pagesize, PROT_READ
    | PROT_WRITE, pkey), 0);
  xsignal (SIGUSR1, &sigusr1_handler);
  xraise (SIGUSR1);
  xsignal (SIGUSR1, SIG_DFL);
  TEST_COMPARE (sigusr1_handler_ran, 1);

  /* Check that access is revoked during signal handling
     with initial rights being set to PKEY_DISABLE_WRITE.  */
  TEST_COMPARE (pkey_set (pkey, PKEY_DISABLE_WRITE), 0);
  xsignal (SIGUSR1, &sigusr1_handler);
  xraise (SIGUSR1);
  xsignal (SIGUSR1, SIG_DFL);
  TEST_COMPARE (sigusr1_handler_ran, 2);

  /* Check that all combinations of PKEY flags used in pkey_set
     result in consistent values obtained via pkey_get.
     Note that whenever flags PKEY_DISABLE_READ and PKEY_DISABLE_WRITE
     are set, the PKEY_DISABLE_ACCESS is also set.  */
  struct
    {
      unsigned int set;
      unsigned int expected;
    } rrs[] =
  {
    {
      PKEY_UNRESTRICTED,
      PKEY_UNRESTRICTED
    },
    {
      PKEY_DISABLE_ACCESS,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
    },
    {
      PKEY_DISABLE_WRITE,
      PKEY_DISABLE_WRITE
    },
    {
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
    },
    {
      PKEY_DISABLE_EXECUTE,
      PKEY_DISABLE_EXECUTE
    },
    {
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_EXECUTE,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
        | PKEY_DISABLE_EXECUTE
    },
    {
      PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE,
      PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE
    },
    {
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
        | PKEY_DISABLE_EXECUTE
    },
    {
      PKEY_DISABLE_READ,
      PKEY_DISABLE_READ
    },
    {
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
    },
    {
      PKEY_DISABLE_WRITE | PKEY_DISABLE_READ,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
    },
    {
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE | PKEY_DISABLE_READ,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE | PKEY_DISABLE_READ
    },
    {
      PKEY_DISABLE_EXECUTE | PKEY_DISABLE_READ,
      PKEY_DISABLE_EXECUTE | PKEY_DISABLE_READ
    },
    {
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_EXECUTE | PKEY_DISABLE_READ,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
        | PKEY_DISABLE_EXECUTE
    },
    {
      PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE | PKEY_DISABLE_READ,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_READ | PKEY_DISABLE_WRITE
        | PKEY_DISABLE_EXECUTE
    },
    {
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE
        | PKEY_DISABLE_READ,
      PKEY_DISABLE_ACCESS | PKEY_DISABLE_WRITE | PKEY_DISABLE_EXECUTE
        | PKEY_DISABLE_READ
    },
  };

  for (int k = 0; k < (array_length (rrs) / 2); k++) {
    TEST_COMPARE (k, rrs[k].set);
    TEST_COMPARE (pkey_set (pkey, rrs[k].set), 0);
    TEST_COMPARE (pkey_get (pkey), rrs[k].expected);
  }

  /* Check that restrictions above maximum allowed value are rejected.  */
  TEST_COMPARE (pkey_set (pkey, 16), -1);
  TEST_COMPARE (errno, EINVAL);

  TEST_COMPARE (pkey_free (pkey), 0);

  xmunmap ((void *) page, pagesize);

  return 0;
}

#include <support/test-driver.c>
