/* Test writing differently sized strings to a memstream.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xmemstream.h>

/* Returns a printable ASCII character based on INDEX.   */
static inline char
char_from_index (unsigned int index)
{
  return ' ' + (index % 95);
}

enum { result_size = 25000 };

static void
run_one_size (unsigned int chunk_size)
{
  char *chunk = xmalloc (chunk_size + 1);

  struct xmemstream mem;
  xopen_memstream (&mem);
  unsigned int written = 0;
  for (unsigned int i = 0; i < result_size; )
    {
      unsigned int to_print = result_size - i;
      if (to_print > chunk_size)
        to_print = chunk_size;
      for (unsigned int j = 0; j < to_print; ++j)
        chunk[j] = char_from_index(i + j);
      chunk[to_print] = '\0';
      fprintf (mem.out, "%s", chunk); /* Needs -fno-builtin-fprintf.  */
      i += to_print;
      written += strlen(chunk);
    }
  xfclose_memstream (&mem);

  TEST_COMPARE (written, result_size);
  TEST_COMPARE (mem.length, result_size);
  TEST_COMPARE (strlen (mem.buffer), result_size);

  for (unsigned int i = 0; i < result_size; ++i)
    TEST_COMPARE (mem.buffer[i], char_from_index (i));

  free (mem.buffer);
  free (chunk);
}

static int
do_test (void)
{
  for (unsigned int chunk_size = 1; chunk_size <= 30; ++ chunk_size)
    run_one_size (chunk_size);

  return 0;
}

#include <support/test-driver.c>
