/* Test that malloc uses mmap when sbrk or brk fails.
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

/* This test sets up an obstruction to ensure that brk/sbrk fails to
   grow the heap, then verifies that malloc uses mmap for allocations
   instead.  */

#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <libc-pointer-arith.h>
#include <support/check.h>
#include <stddef.h>
#include <stdalign.h>

#define LARGE_SIZE (10 * (1 << 20)) // 10 MB
static long page_size;

static int
do_test (void)
{
  /* Get current program break.  */
  void *current_brk = sbrk (0);

  page_size = sysconf (_SC_PAGESIZE);

  /* Round up to the next page boundary.  */
  void *next_page_boundary = PTR_ALIGN_UP (current_brk, page_size);

  /* Place a mapping using mmap at the next page boundary.  */
  void *obstruction_addr
  = mmap (next_page_boundary, page_size, PROT_READ,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

  /* Check if memory obstruction is set up correctly.  */
  TEST_VERIFY_EXIT (obstruction_addr == next_page_boundary);

  /* Try to extend the heap beyond the obstruction using sbrk */
  int *ptr = sbrk (page_size);
  TEST_VERIFY_EXIT (ptr == (void *) -1);

  /* Attempt multiple small allocations using malloc.  */
  for (size_t i = 0; i < page_size / alignof (max_align_t); i++)
    {
      TEST_VERIFY (malloc (alignof (max_align_t)));
    }

  /* Attempt to allocate a large block of memory using malloc.  */
  TEST_VERIFY_EXIT (malloc (LARGE_SIZE) != NULL);

  /* Check if malloc changed current program break.  */
  TEST_VERIFY_EXIT (current_brk == sbrk (0));

  return 0;
}

#include <support/test-driver.c>
