/* Basic tests for mseal.
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

  size_t pagesize = getpagesize ();
  void *p = xmmap (NULL, 4 * pagesize, PROT_READ,
		   MAP_ANONYMOUS | MAP_PRIVATE, -1);
  xmunmap (p + 2 * pagesize, pagesize);

  /* Unaligned address.  */
  TEST_VERIFY_EXIT (mseal (p + 1, pagesize, 0) == -1);
  TEST_COMPARE (errno, EINVAL);

  /* Length too big.  */
  TEST_VERIFY_EXIT (mseal (p, 3 * pagesize, 0) == -1);
  TEST_COMPARE (errno, ENOMEM);

  TEST_VERIFY_EXIT (mseal (p, pagesize, 0) == 0);
  /* Apply the same seal should be idempotent.  */
  TEST_VERIFY_EXIT (mseal (p, pagesize, 0) == 0);

  TEST_VERIFY_EXIT (mprotect (p, pagesize, PROT_WRITE) == -1);
  TEST_COMPARE (errno, EPERM);

  TEST_VERIFY_EXIT (munmap (p, pagesize) == -1);
  TEST_COMPARE (errno, EPERM);

  TEST_VERIFY_EXIT (mremap (p, pagesize, 2 * pagesize, 0) == MAP_FAILED);
  TEST_COMPARE (errno, EPERM);

  TEST_VERIFY_EXIT (madvise (p, pagesize, MADV_DONTNEED) == -1);
  TEST_COMPARE (errno, EPERM);

  xmunmap (p + pagesize, pagesize);
  xmunmap (p + 3 * pagesize, pagesize);

  return 0;
}

#include <support/test-driver.c>
