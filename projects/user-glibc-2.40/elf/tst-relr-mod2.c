/* Test for DT_RELR in a shared library without DT_NEEDED.
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

#include <array_length.h>

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

int
foo (void)
{
  int err = 0;
  for (int i = 0; i < array_length (arr); i++)
    if (!((arr[i] == 0 && val[i] == 0)
	  || (arr[i] == &o && val[i] == 1)
	  || (arr[i] == &x && val[i] == 2)))
      err++;
  return err;
}
