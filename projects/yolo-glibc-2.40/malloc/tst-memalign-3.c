/* Test for memalign chunk reuse.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <malloc.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <array_length.h>
#include <libc-pointer-arith.h>
#include <support/check.h>
#include <support/xthread.h>


typedef struct TestCase {
  size_t size;
  size_t alignment;
  void *ptr1;
  void *ptr2;
} TestCase;

static TestCase tcache_allocs[] = {
  { 24, 32, NULL, NULL },
  { 24, 64, NULL, NULL },
  { 128, 128, NULL, NULL },
  { 500, 128, NULL, NULL }
};
#define TN array_length (tcache_allocs)

static TestCase large_allocs[] = {
  { 23450, 64, NULL, NULL },
  { 23450, 64, NULL, NULL },
  { 23550, 64, NULL, NULL },
  { 23550, 64, NULL, NULL },
  { 23650, 64, NULL, NULL },
  { 23650, 64, NULL, NULL },
  { 33650, 64, NULL, NULL },
  { 33650, 64, NULL, NULL }
};
#define LN array_length (large_allocs)

void *p;

/* Sanity checks, ancillary to the actual test.  */
#define CHECK(p,a) \
  if (p == NULL || !PTR_IS_ALIGNED (p, a)) \
    FAIL_EXIT1 ("NULL or misaligned memory detected.\n");

static void *
mem_test (void *closure)
{
  int i;
  int j;
  int count;
  void *ptr[10];
  void *p;

  /* TCache test.  */
  for (i = 0; i < TN; ++ i)
    {
      size_t sz2;

      tcache_allocs[i].ptr1 = memalign (tcache_allocs[i].alignment, tcache_allocs[i].size);
      CHECK (tcache_allocs[i].ptr1, tcache_allocs[i].alignment);
      sz2 = malloc_usable_size (tcache_allocs[i].ptr1);
      free (tcache_allocs[i].ptr1);

      /* This should return the same chunk as was just free'd.  */
      tcache_allocs[i].ptr2 = memalign (tcache_allocs[i].alignment, sz2);
      CHECK (tcache_allocs[i].ptr2, tcache_allocs[i].alignment);
      free (tcache_allocs[i].ptr2);

      TEST_VERIFY (tcache_allocs[i].ptr1 == tcache_allocs[i].ptr2);
    }

  /* Test for non-head tcache hits.  */
  for (i = 0; i < array_length (ptr); ++ i)
    {
      if (i == 4)
	{
	  ptr[i] = memalign (64, 256);
	  CHECK (ptr[i], 64);
	}
      else
	{
	  ptr[i] = malloc (256);
	  CHECK (ptr[i], 4);
	}
    }
  for (i = 0; i < array_length (ptr); ++ i)
    free (ptr[i]);

  p = memalign (64, 256);
  CHECK (p, 64);

  count = 0;
  for (i = 0; i < 10; ++ i)
    if (ptr[i] == p)
      ++ count;
  free (p);
  TEST_VERIFY (count > 0);

  /* Large bins test.  */

  for (i = 0; i < LN; ++ i)
    {
      large_allocs[i].ptr1 = memalign (large_allocs[i].alignment, large_allocs[i].size);
      CHECK (large_allocs[i].ptr1, large_allocs[i].alignment);
      /* Keep chunks from combining by fragmenting the heap.  */
      p = malloc (512);
      CHECK (p, 4);
    }

  for (i = 0; i < LN; ++ i)
    free (large_allocs[i].ptr1);

  /* Force the unsorted bins to be scanned and moved to small/large
     bins.  */
  p = malloc (60000);

  for (i = 0; i < LN; ++ i)
    {
      large_allocs[i].ptr2 = memalign (large_allocs[i].alignment, large_allocs[i].size);
      CHECK (large_allocs[i].ptr2, large_allocs[i].alignment);
    }

  count = 0;
  for (i = 0; i < LN; ++ i)
    {
      int ok = 0;
      for (j = 0; j < LN; ++ j)
	if (large_allocs[i].ptr1 == large_allocs[j].ptr2)
	  ok = 1;
      if (ok == 1)
	count ++;
    }

  /* The allocation algorithm is complicated outside of the memalign
     logic, so just make sure it's working for most of the
     allocations.  This avoids possible boundary conditions with
     empty/full heaps.  */
  TEST_VERIFY (count > LN / 2);

  return 0;
}

static int
do_test (void)
{
  pthread_t p;

  p = xpthread_create (NULL, mem_test, NULL);
  xpthread_join (p);
  return 0;
}

#include <support/test-driver.c>
