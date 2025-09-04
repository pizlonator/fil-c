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
#include <wchar.h>

/* fa_IR defines to_inpunct for numbers.  */
static const struct input_t
{
  int n;
  const wchar_t str[5];
} inputs[] =
{
  {    1, { 0x000006f1, L'\0' } },
  {    2, { 0x000006f2, L'\0' } },
  {    3, { 0x000006f3, L'\0' } },
  {    4, { 0x000006f4, L'\0' } },
  {    5, { 0x000006f5, L'\0' } },
  {    6, { 0x000006f6, L'\0' } },
  {    7, { 0x000006f7, L'\0' } },
  {    8, { 0x000006f8, L'\0' } },
  {    9, { 0x000006f9, L'\0' } },
  {   10, { 0x000006f1, 0x000006f0, L'\0' } },
  {   11, { 0x000006f1, 0x000006f1, L'\0' } },
  {   12, { 0x000006f1, 0x000006f2, L'\0' } },
  {   13, { 0x000006f1, 0x000006f3, L'\0' } },
  {   14, { 0x000006f1, 0x000006f4, L'\0' } },
  {   15, { 0x000006f1, 0x000006f5, L'\0' } },
  {   16, { 0x000006f1, 0x000006f6, L'\0' } },
  {   17, { 0x000006f1, 0x000006f7, L'\0' } },
  {   18, { 0x000006f1, 0x000006f8, L'\0' } },
  {   19, { 0x000006f1, 0x000006f9, L'\0' } },
  {   20, { 0x000006f2, 0x000006f0, L'\0' } },
  {   30, { 0x000006f3, 0x000006f0, L'\0' } },
  {   40, { 0x000006f4, 0x000006f0, L'\0' } },
  {   50, { 0x000006f5, 0x000006f0, L'\0' } },
  {   60, { 0x000006f6, 0x000006f0, L'\0' } },
  {   70, { 0x000006f7, 0x000006f0, L'\0' } },
  {   80, { 0x000006f8, 0x000006f0, L'\0' } },
  {   90, { 0x000006f9, 0x000006f0, L'\0' } },
  {  100, { 0x000006f1, 0x000006f0, 0x000006f0, L'\0' } },
  { 1000, { 0x000006f1, 0x000006f0, 0x000006f0, 0x000006f0, L'\0' } },
};

static int
do_test (void)
{
  xsetlocale (LC_ALL, "fa_IR.UTF-8");

  for (int i = 0; i < array_length (inputs); i++)
    {
      int n;
      swscanf (inputs[i].str, L"%Id", &n);
      TEST_COMPARE (n, inputs[i].n);
    }

  return 0;
}

#include <support/test-driver.c>
