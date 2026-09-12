/* Basic tests for mseal and pkey.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
#include <sys/mman.h>
#include <support/check.h>
#include <support/xunistd.h>

static int
do_test (void)
{
  TEST_VERIFY (mseal (MAP_FAILED, 0, 0) == -1);
  if (errno == ENOSYS || errno == EPERM)
    FAIL_UNSUPPORTED ("kernel does not support mseal");
  TEST_COMPARE (errno, EINVAL);

  int key = pkey_alloc (0, 0);
  if (key < 0)
    {
      if (errno == ENOSYS)
        FAIL_UNSUPPORTED
          ("kernel does not support memory protection keys");
      if (errno == EINVAL)
        FAIL_UNSUPPORTED
          ("CPU does not support memory protection keys: %m");
      if (errno == ENOSPC)
        FAIL_UNSUPPORTED
          ("no keys available or kernel does not support memory"
           " protection keys");
      FAIL_EXIT1 ("pkey_alloc: %m");
    }

  long pagesize = xsysconf (_SC_PAGESIZE);

  void *page = xmmap (NULL, pagesize, PROT_READ | PROT_WRITE,
		      MAP_ANONYMOUS | MAP_PRIVATE, -1);

  TEST_COMPARE (pkey_mprotect (page, pagesize, PROT_READ | PROT_WRITE,
			       key), 0);

  TEST_VERIFY_EXIT (mseal (page, pagesize, 0) == 0);

  /* For certain destructive madvise behaviours (MADV_DONTNEED,
     MADV_FREE, MADV_DONTNEED_LOCKED, and MADV_WIPEONFORK), mseal
     only blocks the operation if the PKRU denies write.  */
  TEST_VERIFY_EXIT (pkey_set (key, 0) == 0);
  TEST_COMPARE (madvise (page, pagesize, MADV_DONTNEED), 0);

  /* The other mapping operation change are always blocked,
     regardless of PKRU state.  */
  TEST_COMPARE (pkey_mprotect (page, pagesize, PROT_READ, key), -1);
  TEST_COMPARE (errno, EPERM);

  TEST_COMPARE (mprotect (page, pagesize, PROT_READ), -1);
  TEST_COMPARE (errno, EPERM);

  TEST_VERIFY_EXIT (pkey_set (key, PKEY_DISABLE_WRITE) == 0);
  TEST_COMPARE (madvise (page, pagesize, MADV_DONTNEED), -1);
  TEST_COMPARE (errno, EPERM);

  TEST_COMPARE (mprotect (page, pagesize, PROT_READ), -1);
  TEST_COMPARE (errno, EPERM);
  TEST_COMPARE (munmap (page, pagesize),-1);
  TEST_COMPARE (errno, EPERM);

  return 0;
}

#include <support/test-driver.c>
