/* Test regcomp bracket parsing with injected allocation failures (bug 33185).
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

/* This test invokes regcomp multiple times, failing one memory
   allocation in each call.  The function call should fail with
   REG_ESPACE (or succeed if it can recover from the allocation
   failure).  Previously, there was double-free bug.  */

#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <support/check.h>
#include <support/namespace.h>
#include <support/support.h>

/* Data structure allocated via MAP_SHARED, so that writes from the
   subprocess are visible.  */
struct shared_data
{
  /* Number of tracked allocations performed so far.  */
  volatile unsigned int allocation_count;

  /* If this number is reached, one allocation fails.  */
  volatile unsigned int failing_allocation;

  /* The subprocess stores the expected name here.  */
  char name[100];
};

/* Allocation count in shared mapping.  */
static struct shared_data *shared;

/* Returns true if a failure should be injected for this allocation.  */
static bool
fail_this_allocation (void)
{
  if (shared != NULL)
    {
      unsigned int count = shared->allocation_count;
      shared->allocation_count = count + 1;
      return count == shared->failing_allocation;
    }
  else
    return false;
}

/* Failure-injecting wrappers for allocation functions used by glibc.  */

void *
malloc (size_t size)
{
  if (fail_this_allocation ())
    {
      errno = ENOMEM;
      return NULL;
    }
  extern __typeof (malloc) __libc_malloc;
  return __libc_malloc (size);
}

void *
calloc (size_t a, size_t b)
{
  if (fail_this_allocation ())
    {
      errno = ENOMEM;
      return NULL;
    }
  extern __typeof (calloc) __libc_calloc;
  return __libc_calloc (a, b);
}

void *
realloc (void *ptr, size_t size)
{
  if (fail_this_allocation ())
    {
      errno = ENOMEM;
      return NULL;
    }
  extern __typeof (realloc) __libc_realloc;
  return __libc_realloc (ptr, size);
}

/* No-op subprocess to verify that support_isolate_in_subprocess does
   not perform any heap allocations.  */
static void
no_op (void *ignored)
{
}

/* Perform a regcomp call in a subprocess.  Used to count its
   allocations.  */
static void
initialize (void *regexp1)
{
  const char *regexp = regexp1;

  shared->allocation_count = 0;

  regex_t reg;
  TEST_COMPARE (regcomp (&reg, regexp, 0), 0);
}

/* Perform regcomp in a subprocess with fault injection.  */
static void
test_in_subprocess (void *regexp1)
{
  const char *regexp = regexp1;
  unsigned int inject_at = shared->failing_allocation;

  regex_t reg;
  int ret = regcomp (&reg, regexp, 0);

  if (ret != 0)
    {
      TEST_COMPARE (ret, REG_ESPACE);
      printf ("info: allocation %u failure results in return value %d,"
              " error %s (%d)\n",
              inject_at, ret, strerrorname_np (errno), errno);
    }
}

static int
do_test (void)
{
  char regexp[] = "[:alpha:]";

  shared = support_shared_allocate (sizeof (*shared));

  /* Disable fault injection.  */
  shared->failing_allocation = ~0U;

  support_isolate_in_subprocess (no_op, NULL);
  TEST_COMPARE (shared->allocation_count, 0);

  support_isolate_in_subprocess (initialize, regexp);

  /* The number of allocations in the successful case, plus some
     slack.  Once the number of expected allocations is exceeded,
     injecting further failures does not make a difference.  */
  unsigned int maximum_allocation_count = shared->allocation_count;
  printf ("info: successful call performs %u allocations\n",
          maximum_allocation_count);
  maximum_allocation_count += 10;

  for (unsigned int inject_at = 0; inject_at <= maximum_allocation_count;
       ++inject_at)
    {
      shared->allocation_count = 0;
      shared->failing_allocation = inject_at;
      support_isolate_in_subprocess (test_in_subprocess, regexp);
    }

  support_shared_free (shared);

  return 0;
}

#include <support/test-driver.c>
