/* Test scanf for languages with mapping pairs of alternate digits and
   separators.
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

#include <array_length.h>
#include <stdio.h>
#include <support/support.h>
#include <support/check.h>

/* fa_IR defines to_inpunct for numbers.  */
static const struct
{
  int n;
  const char *str;
} inputs[] =
{
  { 1,    "\xdb\xb1" },
  { 2,    "\xdb\xb2" },
  { 3,    "\xdb\xb3" },
  { 4,    "\xdb\xb4" },
  { 5,    "\xdb\xb5" },
  { 6,    "\xdb\xb6" },
  { 7,    "\xdb\xb7" },
  { 8,    "\xdb\xb8" },
  { 9,    "\xdb\xb9" },
  { 10,   "\xdb\xb1\xdb\xb0" },
  { 11,   "\xdb\xb1\xdb\xb1" },
  { 12,   "\xdb\xb1\xdb\xb2" },
  { 13,   "\xdb\xb1\xdb\xb3" },
  { 14,   "\xdb\xb1\xdb\xb4" },
  { 15,   "\xdb\xb1\xdb\xb5" },
  { 16,   "\xdb\xb1\xdb\xb6" },
  { 17,   "\xdb\xb1\xdb\xb7" },
  { 18,   "\xdb\xb1\xdb\xb8" },
  { 19,   "\xdb\xb1\xdb\xb9" },
  { 20,   "\xdb\xb2\xdb\xb0" },
  { 30,   "\xdb\xb3\xdb\xb0" },
  { 40,   "\xdb\xb4\xdb\xb0" },
  { 50,   "\xdb\xb5\xdb\xb0" },
  { 60,   "\xdb\xb6\xdb\xb0" },
  { 70,   "\xdb\xb7\xdb\xb0" },
  { 80,   "\xdb\xb8\xdb\xb0" },
  { 90,   "\xdb\xb9\xdb\xb0" },
  { 100,  "\xdb\xb1\xdb\xb0\xdb\xb0" },
  { 1000, "\xdb\xb1\xdb\xb0\xdb\xb0\xdb\xb0" },
};

static int
do_test (void)
{
  xsetlocale (LC_ALL, "fa_IR.UTF-8");

  for (int i = 0; i < array_length (inputs); i++)
    {
      int n;
      sscanf (inputs[i].str, "%Id", &n);
      TEST_COMPARE (n, inputs[i].n);
    }

  return 0;
}

#include <support/test-driver.c>
