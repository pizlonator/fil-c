/* Support for testing and measuring memcpy functions.
   Copyright (C) 1999-2024 Free Software Foundation, Inc.
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

/* This fail contains the actual memcpy test functions. It is included
   in test-memcpy.c and test-memcpy-large.c.  They are split because
   the tests take a long time to run and splitting them allows for
   simpler parallel testing.  */

#ifndef MEMCPY_RESULT
#define MEMCPY_RESULT(dst, len) dst
#define MIN_PAGE_SIZE 131072
#define TEST_MAIN
#define TEST_NAME "memcpy"
#define TIMEOUT (8 * 60)
#include "test-string.h"

IMPL (memcpy, 1)

/* Naive implementation to verify results.  */
char *
simple_memcpy (char *dst, const char *src, size_t n)
{
  char *ret = dst;
  while (n--)
    *dst++ = *src++;
  return ret;
}

#endif
typedef char *(*proto_t) (char *, const char *, size_t);
typedef uint32_t __attribute__ ((may_alias, aligned (1))) unaligned_uint32_t;

static void
do_one_test (impl_t *impl, char *dst, const char *src, size_t len)
{
  size_t i;

  /* Must clear the destination buffer set by the previous run.  */
  for (i = 0; i < len; i++)
    dst[i] = 0;

  if (CALL (impl, dst, src, len) != MEMCPY_RESULT (dst, len))
    {
      error (0, 0, "Wrong result in function %s %p %p", impl->name,
             CALL (impl, dst, src, len), MEMCPY_RESULT (dst, len));
      ret = 1;
      return;
    }

  if (memcmp (dst, src, len) != 0)
    {
      error (0, 0,
             "Wrong result in function %s dst %p \"%.*s\" src %p \"%.*s\" len "
             "%zu",
             impl->name, dst, (int)len, dst, src, (int)len, src, len);
      ret = 1;
      return;
    }
}

static void
do_test (size_t align1, size_t align2, size_t len)
{
  size_t i, j;
  char *s1, *s2;

  align1 &= 4095;
  if (align1 + len >= page_size)
    return;

  align2 &= 4095;
  if (align2 + len >= page_size)
    return;

  s1 = (char *)(buf1 + align1);
  s2 = (char *)(buf2 + align2);
  for (size_t repeats = 0; repeats < 2; ++repeats)
    {
      for (i = 0, j = 1; i < len; i++, j += 23)
        s1[i] = j;

      FOR_EACH_IMPL (impl, 0)
      do_one_test (impl, s2, s1, len);

      s1 = (char *) (buf2 + align1);
      s2 = (char *) (buf1 + align2);
    }
}

static void
do_test1 (size_t align1, size_t align2, size_t size)
{
  void *large_buf;
  size_t mmap_size, region_size;

  align1 &= (page_size - 1);
  if (align1 == 0)
    align1 = page_size;

  align2 &= (page_size - 1);
  if (align2 == 0)
    align2 = page_size;

  region_size = (size + page_size - 1) & (~(page_size - 1));

  mmap_size = region_size * 2 + 3 * page_size;
  large_buf = mmap (NULL, mmap_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON, -1, 0);
  if (large_buf == MAP_FAILED)
    {
      puts ("Failed to allocate large_buf, skipping do_test");
      return;
    }
  if (mprotect (large_buf + region_size + page_size, page_size, PROT_NONE))
    error (EXIT_FAILURE, errno, "mprotect failed");

  size_t array_size = size / sizeof (uint32_t);
  unaligned_uint32_t *dest = large_buf + align1;
  unaligned_uint32_t *src = large_buf + region_size + 2 * page_size + align2;
  size_t i;
  for (size_t repeats = 0; repeats < 2; repeats++)
    {
      for (i = 0; i < array_size; i++)
        src[i] = (uint32_t)i;
      FOR_EACH_IMPL (impl, 0)
	{
	  memset (dest, -1, size);
	  CALL (impl, (char *)dest, (char *)src, size);
	  if (memcmp (src, dest, size))
	    {
	      for (i = 0; i < array_size; i++)
		if (dest[i] != src[i])
		  {
		    error (0, 0,
			   "Wrong result in function %s dst \"%p\" src \"%p\" "
			   "offset \"%zd\"",
			   impl->name, dest, src, i);
		    ret = 1;
		    munmap ((void *)large_buf, mmap_size);
		    return;
		  }
	    }
	}
      dest = large_buf + region_size + 2 * page_size + align1;
      src = large_buf + align2;
    }
  munmap ((void *)large_buf, mmap_size);
}
