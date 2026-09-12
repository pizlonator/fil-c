/* Test that OOM error suppression works.
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

/* This test reacts to the reject_oom and inject_error environment
   variables.  It is never executed automatically because it can run
   for a very long time on large systems, and is generally stressful
   to the system.  */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support.h>
#include <support/check.h>
#include <sys/mman.h>
#include <unistd.h>

/* If true, support_accept_oom is called.   */
static bool accept_oom;

/* System page size.  Allocations are always at least that large.   */
static size_t page_size;

/* All allocated bytes.  */
static size_t total_bytes;

/* Try to allocate SIZE bytes of memory, and ensure that is backed by
   actual memory.  */
static bool
populate_memory (size_t size)
{
  TEST_COMPARE (size % page_size, 0);
  char *ptr = mmap (NULL, size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED)
    return false;

  if (accept_oom)
    support_accept_oom (true);

  /* Ensure that the kernel allocates backing storage.  Make the pages
     distinct using the total_bytes counter.  */
  for (size_t offset = 0; offset < size; offset += page_size)
    {
      memcpy (ptr + offset, &total_bytes, sizeof (total_bytes));
      total_bytes += page_size;
    }

  if (accept_oom)
    support_accept_oom (false);

  return true;
}

static int
do_test (void)
{
  if (getenv ("oom_test_active") == NULL)
    {
      puts ("info: This test does nothing by default.");
      puts ("info: Set the oom_test_active environment variable to enable it.");
      puts ("info: Consider testing with inject_error and reject_oom as well.");
      return 0;
    }

  accept_oom = getenv ("reject_oom") == NULL;

  page_size = sysconf (_SC_PAGESIZE);
  size_t size = page_size;

  /* The environment variable can be set to trigger a test failure.
     The OOM event should not obscure this error.  */
  TEST_COMPARE_STRING (getenv ("inject_error"), NULL);

  /* Grow the allocation until allocation fails.  */
  while (true)
    {
      size_t new_size = 2 * size;
      if (new_size == 0 || !populate_memory (new_size))
        break;
      size = new_size;
    }

  while (true)
    {
      if (!populate_memory (size))
        {
          /* Decrease size and see if the allocation succeeds.  */
          size /= 2;
          if (size < page_size)
            FAIL_UNSUPPORTED ("could not trigger OOM"
                              " after allocating %zu bytes",
                              total_bytes);
        }
    }

  return 0;
}

#include <support/test-driver.c>
