/* Test <stdbit.h> type-generic macros with -Wconversion.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#include <stdbit.h>
#include <support/check.h>

unsigned char uc;
unsigned short us;
unsigned int ui;
unsigned long int ul;
unsigned long long int ull;

static int
do_test (void)
{
  /* The main point of this test is the compile-time test that there
     are no -Wconversion warnings from the type-generic macros, not
     the runtime execution of those macros.  */
  (void) stdc_leading_zeros (uc);
  (void) stdc_leading_zeros (us);
  (void) stdc_leading_zeros (ui);
  (void) stdc_leading_zeros (ul);
  (void) stdc_leading_zeros (ull);
  (void) stdc_leading_ones (uc);
  (void) stdc_leading_ones (us);
  (void) stdc_leading_ones (ui);
  (void) stdc_leading_ones (ul);
  (void) stdc_leading_ones (ull);
  (void) stdc_trailing_zeros (uc);
  (void) stdc_trailing_zeros (us);
  (void) stdc_trailing_zeros (ui);
  (void) stdc_trailing_zeros (ul);
  (void) stdc_trailing_zeros (ull);
  (void) stdc_trailing_ones (uc);
  (void) stdc_trailing_ones (us);
  (void) stdc_trailing_ones (ui);
  (void) stdc_trailing_ones (ul);
  (void) stdc_trailing_ones (ull);
  (void) stdc_first_leading_zero (uc);
  (void) stdc_first_leading_zero (us);
  (void) stdc_first_leading_zero (ui);
  (void) stdc_first_leading_zero (ul);
  (void) stdc_first_leading_zero (ull);
  (void) stdc_first_leading_one (uc);
  (void) stdc_first_leading_one (us);
  (void) stdc_first_leading_one (ui);
  (void) stdc_first_leading_one (ul);
  (void) stdc_first_leading_one (ull);
  (void) stdc_first_trailing_zero (uc);
  (void) stdc_first_trailing_zero (us);
  (void) stdc_first_trailing_zero (ui);
  (void) stdc_first_trailing_zero (ul);
  (void) stdc_first_trailing_zero (ull);
  (void) stdc_first_trailing_one (uc);
  (void) stdc_first_trailing_one (us);
  (void) stdc_first_trailing_one (ui);
  (void) stdc_first_trailing_one (ul);
  (void) stdc_first_trailing_one (ull);
  (void) stdc_count_zeros (uc);
  (void) stdc_count_zeros (us);
  (void) stdc_count_zeros (ui);
  (void) stdc_count_zeros (ul);
  (void) stdc_count_zeros (ull);
  (void) stdc_count_ones (uc);
  (void) stdc_count_ones (us);
  (void) stdc_count_ones (ui);
  (void) stdc_count_ones (ul);
  (void) stdc_count_ones (ull);
  (void) stdc_has_single_bit (uc);
  (void) stdc_has_single_bit (us);
  (void) stdc_has_single_bit (ui);
  (void) stdc_has_single_bit (ul);
  (void) stdc_has_single_bit (ull);
  (void) stdc_bit_width (uc);
  (void) stdc_bit_width (us);
  (void) stdc_bit_width (ui);
  (void) stdc_bit_width (ul);
  (void) stdc_bit_width (ull);
  (void) stdc_bit_floor (uc);
  (void) stdc_bit_floor (us);
  (void) stdc_bit_floor (ui);
  (void) stdc_bit_floor (ul);
  (void) stdc_bit_floor (ull);
  (void) stdc_bit_ceil (uc);
  (void) stdc_bit_ceil (us);
  (void) stdc_bit_ceil (ui);
  (void) stdc_bit_ceil (ul);
  (void) stdc_bit_ceil (ull);
  return 0;
}

#include <support/test-driver.c>
