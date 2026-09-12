/* Test that free preserves errno.
   Copyright (C) 2020-2026 Free Software Foundation, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <support/check.h>
#include <support/support.h>
#include <support/temp_file.h>

/* The __attribute__ ((weak)) prevents a GCC optimization.  Without
   it, GCC would "know" that errno is unchanged by calling free (ptr),
   when ptr was the result of a malloc call in the same function.  */
int __attribute__ ((weak))
get_errno (void)
{
  return errno;
}

static int
do_test (void)
{
  /* Check that free() preserves errno.  */
  {
    errno = 1789; /* Liberté, égalité, fraternité.  */
    free (NULL);
    TEST_VERIFY (get_errno () == 1789);
  }
  { /* Large memory allocations, to force mmap.  */
    enum { N = 2 };
    void * volatile ptrs[N];
    size_t i;
    for (i = 0; i < N; i++)
      ptrs[i] = xmalloc (5318153);
    for (i = 0; i < N; i++)
      {
        errno = 1789;
        free (ptrs[i]);
        TEST_VERIFY (get_errno () == 1789);
      }
  }

  /* Test a less common code path.
     When malloc() is based on mmap(), free() can sometimes call munmap().
     munmap() usually succeeds, but fails in a particular situation: when
       - it has to unmap the middle part of a VMA, and
       - the number of VMAs of a process is limited and the limit is
         already reached.
     On Linux, the default VMA count limit is 65536 although many systems
     might override this.  For this test, irrespective of the system, we
     try to create up to 65536 mappings in order to attempt to hit the
     limit.  */
  {
    /* We expect the kernel to coalesce the VMAs for these large mallocs
       (which will be mmap'd by malloc due to their size).  */
    size_t big_size = 0x3000000;
    void * volatile block1 = xmalloc (big_size - 100);
    void * volatile block2 = xmalloc (big_size - 100);
    void * volatile block3 = xmalloc (big_size - 100);

    /* If block2 lands between block1 and block3, we can continue the test
       since it depends on being able to free block2 to cause an munmap
       failure.  */
    if (((block2 > block1) && (block2 > block3))
        || ((block2 < block1) && (block2 < block3)))
      printf
        ("warning: block2 was not allocated between block1 and block3\n");

    /* We will map this fd repeatedly to consume VMA mappings.  */
    int fd = create_temp_file ("tst-free-errno", NULL);
    if (fd < 0)
      FAIL_EXIT1 ("cannot create temporary file for mmap'ing");

    /* Now add as many mappings as we can.
       Stop at 65536, in order not to crash the machine (in case the
       limit has been increased by the system administrator).  */
    size_t pagesize = getpagesize ();
    for (int i = 0; i < 65536; i++)
      if (mmap (NULL, pagesize, PROT_READ, MAP_FILE | MAP_PRIVATE,
                fd, 0)
          == MAP_FAILED)
        break;
    /* Now the number of VMAs of this process has hopefully attained
       its limit.  */

    errno = 1789;
    /* This call to free() is supposed to call munmap, which should
       fail because the fragmentation of a bigger coalesced VMA will
       lead to an increase in the number of VMAs which we already
       maxed out.  */
    free (block2);
    TEST_VERIFY (get_errno () == 1789);
  }

  return 0;
}

#include <support/test-driver.c>
