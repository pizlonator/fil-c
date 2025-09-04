/* Test and measure memchr functions.
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

#include <assert.h>
#include <support/xunistd.h>

#define TEST_MAIN
#define TEST_NAME "rawmemchr"
#include "test-string.h"

typedef char *(*proto_t) (const char *, int);
char *simple_rawmemchr (const char *, int);

IMPL (simple_rawmemchr, 0)
IMPL (rawmemchr, 1)

char *
simple_rawmemchr (const char *s, int c)
{
  while (1)
    if (*s++ == (char) c)
      return (char *) s - 1;
  return NULL;
}

static void
do_one_test (impl_t *impl, const char *s, int c, char *exp_res)
{
  char *res = CALL (impl, s, c);
  if (res != exp_res)
    {
      error (0, 0, "Wrong result in function %s %p %p", impl->name,
	     res, exp_res);
      ret = 1;
      return;
    }
}

static void
do_test_bz29234 (void)
{
  size_t i, j;
  char *ptr_start;
  char *buf = xmmap (0, 8192, PROT_READ | PROT_WRITE,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1);

  memset (buf, -1, 8192);

  ptr_start = buf + 4096 - 8;

  /* Out of range matches before the start of a page. */
  memset (ptr_start - 8, 0x1, 8);

  for (j = 0; j < 8; ++j)
    {
      for (i = 0; i < 128; ++i)
	{
	  ptr_start[i + j] = 0x1;

	  FOR_EACH_IMPL (impl, 0)
	    do_one_test (impl, (char *) (ptr_start + j), 0x1,
			 ptr_start + i + j);

	  ptr_start[i + j] = 0xff;
	}
    }

  xmunmap (buf, 8192);
}

static void
do_test (size_t align, size_t pos, size_t len, int seek_char)
{
  size_t i;
  char *result;

  align &= getpagesize () - 1;
  if (align + len >= page_size)
    return;

  for (i = 0; i < len; ++i)
    {
      buf1[align + i] = 1 + 23 * i % 127;
      if (buf1[align + i] == seek_char)
	buf1[align + i] = seek_char + 1;
    }
  buf1[align + len] = 0;

  assert (pos < len);

  buf1[align + pos] = seek_char;
  buf1[align + len] = -seek_char;
  result = (char *) (buf1 + align + pos);

  FOR_EACH_IMPL (impl, 0)
    do_one_test (impl, (char *) (buf1 + align), seek_char, result);
}

static void
do_random_tests (void)
{
  size_t i, j, n, align, pos, len;
  int seek_char;
  char *result;
  unsigned char *p = buf1 + page_size - 512;

  for (n = 0; n < ITERATIONS; n++)
    {
      align = random () & 15;
      pos = random () & 511;
      if (pos + align >= 512)
	pos = 511 - align - (random () & 7);
      len = random () & 511;
      if (len + align >= 512)
	len = 512 - align - (random () & 7);
      if (pos >= len)
	continue;
      seek_char = random () & 255;
      j = len + align + 64;
      if (j > 512)
	j = 512;

      for (i = 0; i < j; i++)
	{
	  if (i == pos + align)
	    p[i] = seek_char;
	  else
	    {
	      p[i] = random () & 255;
	      if (i < pos + align && p[i] == seek_char)
		p[i] = seek_char + 13;
	    }
	}

      if (align)
	{
	  p[align - 1] = seek_char;
	  if (align > 4)
	    p[align - 4] = seek_char;
	}

      assert (pos < len);
      size_t r = random ();
      if ((r & 31) == 0)
	len = ~(uintptr_t) (p + align) - ((r >> 5) & 31);
      result = (char *) (p + pos + align);

      FOR_EACH_IMPL (impl, 1)
	if (CALL (impl, (char *) (p + align), seek_char) != result)
	  {
	    error (0, 0, "Iteration %zd - wrong result in function %s (%zd, %d, %zd, %zd) %p != %p, p %p",
		   n, impl->name, align, seek_char, len, pos,
		   CALL (impl, (char *) (p + align), seek_char),
		   result, p);
	    ret = 1;
	  }

      if (align)
	{
	  p[align - 1] = seek_char;
	  if (align > 4)
	    p[align - 4] = seek_char;
	}
    }
}

int
test_main (void)
{
  size_t i;

  test_init ();

  printf ("%20s", "");
  FOR_EACH_IMPL (impl, 0)
    printf ("\t%s", impl->name);
  putchar ('\n');

  for (i = 1; i < 7; ++i)
    {
      do_test (0, 16 << i, 2048, 23);
      do_test (i, 64, 256, 23);
      do_test (0, 16 << i, 2048, 0);
      do_test (i, 64, 256, 0);

      do_test (getpagesize () - i, 64, 256, 23);
      do_test (getpagesize () - i, 64, 256, 0);
    }
  for (i = 1; i < 32; ++i)
    {
      do_test (0, i, i + 1, 23);
      do_test (0, i, i + 1, 0);

      do_test (getpagesize () - 7, i, i + 1, 23);
      do_test (getpagesize () - i / 2, i, i + 1, 23);
      do_test (getpagesize () - i, i, i + 1, 23);
    }

  do_random_tests ();
  do_test_bz29234 ();
  return ret;
}

#include <support/test-driver.c>
