/* Basic tests for DT_RELR.
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

#include <link.h>
#include <stdbool.h>
#include <array_length.h>
#include <support/check.h>

static int o, x;

#define ELEMS O O O O O O O O X X X X X X X O O X O O X X X E X E E O X O E
#define E 0,

#define O &o,
#define X &x,
void *arr[] = { ELEMS };
#undef O
#undef X

#define O 1,
#define X 2,
static char val[] = { ELEMS };

static int
do_test (void)
{
  ElfW(Dyn) *d = _DYNAMIC;
  if (d)
    {
      bool has_relr = false;
      for (; d->d_tag != DT_NULL; d++)
	if (d->d_tag == DT_RELR)
	  has_relr = true;

#if defined __PIE__ || defined __pie__ || defined PIE || defined pie
      TEST_VERIFY (has_relr);
#else
      TEST_VERIFY (!has_relr);
#endif
    }

  for (int i = 0; i < array_length (arr); i++)
    TEST_VERIFY ((arr[i] == 0 && val[i] == 0)
		 || (arr[i] == &o && val[i] == 1)
		 || (arr[i] == &x && val[i] == 2));

  return 0;
}

#include <support/test-driver.c>
