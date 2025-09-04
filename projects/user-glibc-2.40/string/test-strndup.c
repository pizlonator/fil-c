/* Test strndup functions.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <support/check.h>

#define TEST_MAIN
#include "test-string.h"

static void
do_one_test (const char *src, size_t len, size_t n)
{
  char *dst = strndup (src, n);
  size_t s = (len > n ? n: len) * sizeof (char);

  TEST_COMPARE_BLOB (dst, s, src, s);

  free (dst);
}

static void
do_test (size_t align1, size_t align2, size_t len, size_t n, int max_char)
{
  size_t i;
  char *s1;

  align1 &= 7;
  if ((align1 + len) * sizeof (char) >= page_size)
    return;

  align2 &= 7;
  if ((align2 + len) * sizeof (char) >= page_size)
    return;

  s1 = (char *) (buf1) + align1;

  for (i = 0; i < len; ++i)
    s1[i] = 32 + 23 * i % (max_char - 32);
  s1[len] = 0;
  for (i = len + 1; (i + align1) * sizeof (char) < page_size && i < len + 64;
       ++i)
    s1[i] = 32 + 32 * i % (max_char - 32);

  do_one_test (s1, len, n);
}

static void
do_page_tests (void)
{
  char *s1;
  const size_t maxoffset = 64;

  /* Put s1 at the maxoffset from the edge of buf1's last page.  */
  s1 = (char *) buf1 + BUF1PAGES * page_size / sizeof (char) - maxoffset;

  memset (s1, 'a', maxoffset - 1);
  s1[maxoffset - 1] = '\0';

  /* Both strings are bounded to a page with read/write access and the next
     page is protected with PROT_NONE (meaning that any access outside of the
     page regions will trigger an invalid memory access).

     The loop copies the string s1 for all possible offsets up to maxoffset
     for both inputs with a size larger than s1 (so memory access outside the
     expected memory regions might trigger invalid access).  */

  for (size_t off1 = 0; off1 < maxoffset; off1++)
    for (size_t off2 = 0; off2 < maxoffset; off2++)
      do_one_test (s1 + off1, maxoffset - off1 - 1,
		   maxoffset + (maxoffset - off2));
}

static void
do_random_tests (void)
{
  size_t i, j, n, align1, align2, len, size, mode;
  char *p1 = (char *) (buf1 + page_size) - 512;
  char *res;

  for (n = 0; n < ITERATIONS; n++)
    {
      mode = random ();
      if (mode & 1)
	{
	  size = random () & 255;
	  align1 = 512 - size - (random () & 15);
	  if (mode & 2)
	    align2 = align1 - (random () & 24);
	  else
	    align2 = align1 - (random () & 31);
	  if (mode & 4)
	    {
	      j = align1;
	      align1 = align2;
	      align2 = j;
	    }
	  if (mode & 8)
	    len = size - (random () & 31);
	  else
	    len = 512;
	  if (len >= 512)
	    len = random () & 511;
	}
      else
	{
	  align1 = random () & 31;
	  if (mode & 2)
	    align2 = random () & 31;
	  else
	    align2 = align1 + (random () & 24);
	  len = random () & 511;
	  j = align1;
	  if (align2 > j)
	    j = align2;
	  if (mode & 4)
	    {
	      size = random () & 511;
	      if (size + j > 512)
		size = 512 - j - (random () & 31);
	    }
	  else
	    size = 512 - j;
	  if ((mode & 8) && len + j >= 512)
	    len = 512 - j - (random () & 7);
	}
      j = len + align1 + 64;
      if (j > 512)
	j = 512;
      for (i = 0; i < j; i++)
	{
	  if (i == len + align1)
	    p1[i] = 0;
	  else
	    {
	      p1[i] = random () & CHAR_MAX;
	      if (i >= align1 && i < len + align1 && !p1[i])
		p1[i] = (random () & 127) + 3;
	    }
	}

	res = (char *) strndup ((char *) (p1 + align1), size);
	j = len + 1;
	if (size < j)
	  j = size;
	TEST_COMPARE_BLOB (res, j, (char *) (p1 + align1), j);
	free (res);
    }
}

int
test_main (void)
{
  size_t i;

  test_init ();

  printf ("%28s", "");
  printf ("\t%s", "strndup");
  putchar ('\n');

  for (i = 1; i < 8; ++i)
    {
      do_test (i, i, 16, 16, 127);
      do_test (i, i, 16, 16, CHAR_MAX);
      do_test (i, 2 * i, 16, 16, 127);
      do_test (2 * i, i, 16, 16, CHAR_MAX);
      do_test (8 - i, 2 * i, 1 << i, 2 << i, 127);
      do_test (2 * i, 8 - i, 2 << i, 1 << i, 127);
      do_test (8 - i, 2 * i, 1 << i, 2 << i, CHAR_MAX);
      do_test (2 * i, 8 - i, 2 << i, 1 << i, CHAR_MAX);
    }

  for (i = 1; i < 8; ++i)
    {
      do_test (0, 0, 4 << i, 8 << i, 127);
      do_test (0, 0, 16 << i, 8 << i, 127);
      do_test (8 - i, 2 * i, 4 << i, 8 << i, 127);
      do_test (8 - i, 2 * i, 16 << i, 8 << i, 127);
    }

  do_random_tests ();
  do_page_tests ();
  return ret;
}

#include <support/test-driver.c>
