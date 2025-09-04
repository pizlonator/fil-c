/* Test <stdbit.h> type-generic macros with compiler __builtin_stdc_* support.
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
#include <limits.h>
#include <support/check.h>

#if __glibc_has_builtin (__builtin_stdc_leading_zeros) \
    && __glibc_has_builtin (__builtin_stdc_leading_ones) \
    && __glibc_has_builtin (__builtin_stdc_trailing_zeros) \
    && __glibc_has_builtin (__builtin_stdc_trailing_ones) \
    && __glibc_has_builtin (__builtin_stdc_first_leading_zero) \
    && __glibc_has_builtin (__builtin_stdc_first_leading_one) \
    && __glibc_has_builtin (__builtin_stdc_first_trailing_zero) \
    && __glibc_has_builtin (__builtin_stdc_first_trailing_one) \
    && __glibc_has_builtin (__builtin_stdc_count_zeros) \
    && __glibc_has_builtin (__builtin_stdc_count_ones) \
    && __glibc_has_builtin (__builtin_stdc_has_single_bit) \
    && __glibc_has_builtin (__builtin_stdc_bit_width) \
    && __glibc_has_builtin (__builtin_stdc_bit_floor) \
    && __glibc_has_builtin (__builtin_stdc_bit_ceil)

# if !defined (BITINT_MAXWIDTH) && defined (__BITINT_MAXWIDTH__)
#  define BITINT_MAXWIDTH __BITINT_MAXWIDTH__
# endif

typedef unsigned char uc;
typedef unsigned short us;
typedef unsigned int ui;
typedef unsigned long int ul;
typedef unsigned long long int ull;

# define expr_has_type(e, t) _Generic (e, default : 0, t : 1)

static int
do_test (void)
{
  TEST_COMPARE (stdc_leading_zeros ((uc) 0), CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_leading_zeros ((us) 0), sizeof (short) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros ((us) 0), ui), 1);
  TEST_COMPARE (stdc_leading_zeros (0U), sizeof (int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros (0U), ui), 1);
  TEST_COMPARE (stdc_leading_zeros (0UL), sizeof (long int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros (0UL), ui), 1);
  TEST_COMPARE (stdc_leading_zeros (0ULL), sizeof (long long int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros (0ULL), ui), 1);
  TEST_COMPARE (stdc_leading_zeros ((uc) ~0U), 0);
  TEST_COMPARE (stdc_leading_zeros ((us) ~0U), 0);
  TEST_COMPARE (stdc_leading_zeros (~0U), 0);
  TEST_COMPARE (stdc_leading_zeros (~0UL), 0);
  TEST_COMPARE (stdc_leading_zeros (~0ULL), 0);
  TEST_COMPARE (stdc_leading_zeros ((uc) 3), CHAR_BIT - 2);
  TEST_COMPARE (stdc_leading_zeros ((us) 9), sizeof (short) * CHAR_BIT - 4);
  TEST_COMPARE (stdc_leading_zeros (34U), sizeof (int) * CHAR_BIT - 6);
  TEST_COMPARE (stdc_leading_zeros (130UL), sizeof (long int) * CHAR_BIT - 8);
  TEST_COMPARE (stdc_leading_zeros (512ULL),
		sizeof (long long int) * CHAR_BIT - 10);
  TEST_COMPARE (stdc_leading_ones ((uc) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_leading_ones ((us) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones ((us) 0), ui), 1);
  TEST_COMPARE (stdc_leading_ones (0U), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones (0U), ui), 1);
  TEST_COMPARE (stdc_leading_ones (0UL), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones (0UL), ui), 1);
  TEST_COMPARE (stdc_leading_ones (0ULL), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones (0ULL), ui), 1);
  TEST_COMPARE (stdc_leading_ones ((uc) ~0U), CHAR_BIT);
  TEST_COMPARE (stdc_leading_ones ((us) ~0U), sizeof (short) * CHAR_BIT);
  TEST_COMPARE (stdc_leading_ones (~0U), sizeof (int) * CHAR_BIT);
  TEST_COMPARE (stdc_leading_ones (~0UL), sizeof (long int) * CHAR_BIT);
  TEST_COMPARE (stdc_leading_ones (~0ULL), sizeof (long long int) * CHAR_BIT);
  TEST_COMPARE (stdc_leading_ones ((uc) ~3), CHAR_BIT - 2);
  TEST_COMPARE (stdc_leading_ones ((us) ~9), sizeof (short) * CHAR_BIT - 4);
  TEST_COMPARE (stdc_leading_ones (~34U), sizeof (int) * CHAR_BIT - 6);
  TEST_COMPARE (stdc_leading_ones (~130UL), sizeof (long int) * CHAR_BIT - 8);
  TEST_COMPARE (stdc_leading_ones (~512ULL),
		sizeof (long long int) * CHAR_BIT - 10);
  TEST_COMPARE (stdc_trailing_zeros ((uc) 0), CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_trailing_zeros ((us) 0), sizeof (short) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros ((us) 0), ui), 1);
  TEST_COMPARE (stdc_trailing_zeros (0U), sizeof (int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros (0U), ui), 1);
  TEST_COMPARE (stdc_trailing_zeros (0UL), sizeof (long int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros (0UL), ui), 1);
  TEST_COMPARE (stdc_trailing_zeros (0ULL), sizeof (long long int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros (0ULL), ui), 1);
  TEST_COMPARE (stdc_trailing_zeros ((uc) ~0U), 0);
  TEST_COMPARE (stdc_trailing_zeros ((us) ~0U), 0);
  TEST_COMPARE (stdc_trailing_zeros (~0U), 0);
  TEST_COMPARE (stdc_trailing_zeros (~0UL), 0);
  TEST_COMPARE (stdc_trailing_zeros (~0ULL), 0);
  TEST_COMPARE (stdc_trailing_zeros ((uc) 2), 1);
  TEST_COMPARE (stdc_trailing_zeros ((us) 24), 3);
  TEST_COMPARE (stdc_trailing_zeros (32U), 5);
  TEST_COMPARE (stdc_trailing_zeros (128UL), 7);
  TEST_COMPARE (stdc_trailing_zeros (512ULL), 9);
  TEST_COMPARE (stdc_trailing_ones ((uc) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_trailing_ones ((us) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones ((us) 0), ui), 1);
  TEST_COMPARE (stdc_trailing_ones (0U), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones (0U), ui), 1);
  TEST_COMPARE (stdc_trailing_ones (0UL), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones (0UL), ui), 1);
  TEST_COMPARE (stdc_trailing_ones (0ULL), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones (0ULL), ui), 1);
  TEST_COMPARE (stdc_trailing_ones ((uc) ~0U), CHAR_BIT);
  TEST_COMPARE (stdc_trailing_ones ((us) ~0U), sizeof (short) * CHAR_BIT);
  TEST_COMPARE (stdc_trailing_ones (~0U), sizeof (int) * CHAR_BIT);
  TEST_COMPARE (stdc_trailing_ones (~0UL), sizeof (long int) * CHAR_BIT);
  TEST_COMPARE (stdc_trailing_ones (~0ULL), sizeof (long long int) * CHAR_BIT);
  TEST_COMPARE (stdc_trailing_ones ((uc) 5), 1);
  TEST_COMPARE (stdc_trailing_ones ((us) 15), 4);
  TEST_COMPARE (stdc_trailing_ones (127U), 7);
  TEST_COMPARE (stdc_trailing_ones (511UL), 9);
  TEST_COMPARE (stdc_trailing_ones (~0ULL >> 2),
		sizeof (long long int) * CHAR_BIT - 2);
  TEST_COMPARE (stdc_first_leading_zero ((uc) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero ((us) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero ((us) 0), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero (0U), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero (0U), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero (0UL), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero (0UL), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero (0ULL), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero (0ULL), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero ((uc) ~0U), 0);
  TEST_COMPARE (stdc_first_leading_zero ((us) ~0U), 0);
  TEST_COMPARE (stdc_first_leading_zero (~0U), 0);
  TEST_COMPARE (stdc_first_leading_zero (~0UL), 0);
  TEST_COMPARE (stdc_first_leading_zero (~0ULL), 0);
  TEST_COMPARE (stdc_first_leading_zero ((uc) ~3U), CHAR_BIT - 1);
  TEST_COMPARE (stdc_first_leading_zero ((us) ~15U),
		sizeof (short) * CHAR_BIT - 3);
  TEST_COMPARE (stdc_first_leading_zero (~63U), sizeof (int) * CHAR_BIT - 5);
  TEST_COMPARE (stdc_first_leading_zero (~255UL),
		sizeof (long int) * CHAR_BIT - 7);
  TEST_COMPARE (stdc_first_leading_zero (~1023ULL),
		sizeof (long long int) * CHAR_BIT - 9);
  TEST_COMPARE (stdc_first_leading_one ((uc) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_first_leading_one ((us) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one ((us) 0), ui), 1);
  TEST_COMPARE (stdc_first_leading_one (0U), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one (0U), ui), 1);
  TEST_COMPARE (stdc_first_leading_one (0UL), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one (0UL), ui), 1);
  TEST_COMPARE (stdc_first_leading_one (0ULL), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one (0ULL), ui), 1);
  TEST_COMPARE (stdc_first_leading_one ((uc) ~0U), 1);
  TEST_COMPARE (stdc_first_leading_one ((us) ~0U), 1);
  TEST_COMPARE (stdc_first_leading_one (~0U), 1);
  TEST_COMPARE (stdc_first_leading_one (~0UL), 1);
  TEST_COMPARE (stdc_first_leading_one (~0ULL), 1);
  TEST_COMPARE (stdc_first_leading_one ((uc) 3), CHAR_BIT - 1);
  TEST_COMPARE (stdc_first_leading_one ((us) 9),
		sizeof (short) * CHAR_BIT - 3);
  TEST_COMPARE (stdc_first_leading_one (34U), sizeof (int) * CHAR_BIT - 5);
  TEST_COMPARE (stdc_first_leading_one (130UL),
		sizeof (long int) * CHAR_BIT - 7);
  TEST_COMPARE (stdc_first_leading_one (512ULL),
		sizeof (long long int) * CHAR_BIT - 9);
  TEST_COMPARE (stdc_first_trailing_zero ((uc) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero ((us) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero ((us) 0), ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero (0U), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero (0U), ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero (0UL), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero (0UL), ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero (0ULL), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero (0ULL), ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero ((uc) ~0U), 0);
  TEST_COMPARE (stdc_first_trailing_zero ((us) ~0U), 0);
  TEST_COMPARE (stdc_first_trailing_zero (~0U), 0);
  TEST_COMPARE (stdc_first_trailing_zero (~0UL), 0);
  TEST_COMPARE (stdc_first_trailing_zero (~0ULL), 0);
  TEST_COMPARE (stdc_first_trailing_zero ((uc) 2), 1);
  TEST_COMPARE (stdc_first_trailing_zero ((us) 15), 5);
  TEST_COMPARE (stdc_first_trailing_zero (63U), 7);
  TEST_COMPARE (stdc_first_trailing_zero (128UL), 1);
  TEST_COMPARE (stdc_first_trailing_zero (511ULL), 10);
  TEST_COMPARE (stdc_first_trailing_one ((uc) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one ((us) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one ((us) 0), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one (0U), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one (0U), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one (0UL), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one (0UL), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one (0ULL), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one (0ULL), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one ((uc) ~0U), 1);
  TEST_COMPARE (stdc_first_trailing_one ((us) ~0U), 1);
  TEST_COMPARE (stdc_first_trailing_one (~0U), 1);
  TEST_COMPARE (stdc_first_trailing_one (~0UL), 1);
  TEST_COMPARE (stdc_first_trailing_one (~0ULL), 1);
  TEST_COMPARE (stdc_first_trailing_one ((uc) 4), 3);
  TEST_COMPARE (stdc_first_trailing_one ((us) 96), 6);
  TEST_COMPARE (stdc_first_trailing_one (127U), 1);
  TEST_COMPARE (stdc_first_trailing_one (511UL), 1);
  TEST_COMPARE (stdc_first_trailing_one (~0ULL << 12), 13);
  TEST_COMPARE (stdc_count_zeros ((uc) 0), CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_count_zeros ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_count_zeros ((us) 0), sizeof (short) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_count_zeros ((us) 0), ui), 1);
  TEST_COMPARE (stdc_count_zeros (0U), sizeof (int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_count_zeros (0U), ui), 1);
  TEST_COMPARE (stdc_count_zeros (0UL), sizeof (long int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_count_zeros (0UL), ui), 1);
  TEST_COMPARE (stdc_count_zeros (0ULL), sizeof (long long int) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_count_zeros (0ULL), ui), 1);
  TEST_COMPARE (stdc_count_zeros ((uc) ~0U), 0);
  TEST_COMPARE (stdc_count_zeros ((us) ~0U), 0);
  TEST_COMPARE (stdc_count_zeros (~0U), 0);
  TEST_COMPARE (stdc_count_zeros (~0UL), 0);
  TEST_COMPARE (stdc_count_zeros (~0ULL), 0);
  TEST_COMPARE (stdc_count_zeros ((uc) 1U), CHAR_BIT - 1);
  TEST_COMPARE (stdc_count_zeros ((us) 42), sizeof (short) * CHAR_BIT - 3);
  TEST_COMPARE (stdc_count_zeros (291U), sizeof (int) * CHAR_BIT - 4);
  TEST_COMPARE (stdc_count_zeros (~1315UL), 5);
  TEST_COMPARE (stdc_count_zeros (3363ULL),
		sizeof (long long int) * CHAR_BIT - 6);
  TEST_COMPARE (stdc_count_ones ((uc) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_count_ones ((us) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones ((us) 0), ui), 1);
  TEST_COMPARE (stdc_count_ones (0U), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones (0U), ui), 1);
  TEST_COMPARE (stdc_count_ones (0UL), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones (0UL), ui), 1);
  TEST_COMPARE (stdc_count_ones (0ULL), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones (0ULL), ui), 1);
  TEST_COMPARE (stdc_count_ones ((uc) ~0U), CHAR_BIT);
  TEST_COMPARE (stdc_count_ones ((us) ~0U), sizeof (short) * CHAR_BIT);
  TEST_COMPARE (stdc_count_ones (~0U), sizeof (int) * CHAR_BIT);
  TEST_COMPARE (stdc_count_ones (~0UL), sizeof (long int) * CHAR_BIT);
  TEST_COMPARE (stdc_count_ones (~0ULL), sizeof (long long int) * CHAR_BIT);
  TEST_COMPARE (stdc_count_ones ((uc) ~1U), CHAR_BIT - 1);
  TEST_COMPARE (stdc_count_ones ((us) ~42), sizeof (short) * CHAR_BIT - 3);
  TEST_COMPARE (stdc_count_ones (~291U), sizeof (int) * CHAR_BIT - 4);
  TEST_COMPARE (stdc_count_ones (1315UL), 5);
  TEST_COMPARE (stdc_count_ones (~3363ULL),
		sizeof (long long int) * CHAR_BIT - 6);
  TEST_COMPARE (stdc_has_single_bit ((uc) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit ((uc) 0), _Bool), 1);
  TEST_COMPARE (stdc_has_single_bit ((us) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit ((us) 0), _Bool), 1);
  TEST_COMPARE (stdc_has_single_bit (0U), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit (0U), _Bool), 1);
  TEST_COMPARE (stdc_has_single_bit (0UL), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit (0UL), _Bool), 1);
  TEST_COMPARE (stdc_has_single_bit (0ULL), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit (0ULL), _Bool), 1);
  TEST_COMPARE (stdc_has_single_bit ((uc) 2), 1);
  TEST_COMPARE (stdc_has_single_bit ((us) 8), 1);
  TEST_COMPARE (stdc_has_single_bit (32U), 1);
  TEST_COMPARE (stdc_has_single_bit (128UL), 1);
  TEST_COMPARE (stdc_has_single_bit (512ULL), 1);
  TEST_COMPARE (stdc_has_single_bit ((uc) 7), 0);
  TEST_COMPARE (stdc_has_single_bit ((us) 96), 0);
  TEST_COMPARE (stdc_has_single_bit (513U), 0);
  TEST_COMPARE (stdc_has_single_bit (1022UL), 0);
  TEST_COMPARE (stdc_has_single_bit (12ULL), 0);
  TEST_COMPARE (stdc_bit_width ((uc) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width ((uc) 0), ui), 1);
  TEST_COMPARE (stdc_bit_width ((us) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width ((us) 0), ui), 1);
  TEST_COMPARE (stdc_bit_width (0U), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width (0U), ui), 1);
  TEST_COMPARE (stdc_bit_width (0UL), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width (0UL), ui), 1);
  TEST_COMPARE (stdc_bit_width (0ULL), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width (0ULL), ui), 1);
  TEST_COMPARE (stdc_bit_width ((uc) ~0U), CHAR_BIT);
  TEST_COMPARE (stdc_bit_width ((us) ~0U), sizeof (short) * CHAR_BIT);
  TEST_COMPARE (stdc_bit_width (~0U), sizeof (int) * CHAR_BIT);
  TEST_COMPARE (stdc_bit_width (~0UL), sizeof (long int) * CHAR_BIT);
  TEST_COMPARE (stdc_bit_width (~0ULL), sizeof (long long int) * CHAR_BIT);
  TEST_COMPARE (stdc_bit_width ((uc) ((uc) ~0U >> 1)), CHAR_BIT - 1);
  TEST_COMPARE (stdc_bit_width ((uc) 6), 3);
  TEST_COMPARE (stdc_bit_width ((us) 12U), 4);
  TEST_COMPARE (stdc_bit_width ((us) ((us) ~0U >> 5)),
		sizeof (short) * CHAR_BIT - 5);
  TEST_COMPARE (stdc_bit_width (137U), 8);
  TEST_COMPARE (stdc_bit_width (269U), 9);
  TEST_COMPARE (stdc_bit_width (39UL), 6);
  TEST_COMPARE (stdc_bit_width (~0UL >> 2), sizeof (long int) * CHAR_BIT - 2);
  TEST_COMPARE (stdc_bit_width (1023ULL), 10);
  TEST_COMPARE (stdc_bit_width (1024ULL), 11);
  TEST_COMPARE (stdc_bit_floor ((uc) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_floor ((uc) 0), uc), 1);
  TEST_COMPARE (stdc_bit_floor ((us) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_floor ((us) 0), us), 1);
  TEST_COMPARE (stdc_bit_floor (0U), 0U);
  TEST_COMPARE (expr_has_type (stdc_bit_floor (0U), ui), 1);
  TEST_COMPARE (stdc_bit_floor (0UL), 0UL);
  TEST_COMPARE (expr_has_type (stdc_bit_floor (0UL), ul), 1);
  TEST_COMPARE (stdc_bit_floor (0ULL), 0ULL);
  TEST_COMPARE (expr_has_type (stdc_bit_floor (0ULL), ull), 1);
  TEST_COMPARE (stdc_bit_floor ((uc) ~0U), (1U << (CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_floor ((us) ~0U),
		(1U << (sizeof (short) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_floor (~0U), (1U << (sizeof (int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_floor (~0UL),
		(1UL << (sizeof (long int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_floor (~0ULL),
		(1ULL << (sizeof (long long int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_floor ((uc) 4), 4);
  TEST_COMPARE (stdc_bit_floor ((uc) 7), 4);
  TEST_COMPARE (stdc_bit_floor ((us) 8U), 8);
  TEST_COMPARE (stdc_bit_floor ((us) 31U), 16);
  TEST_COMPARE (stdc_bit_floor (137U), 128U);
  TEST_COMPARE (stdc_bit_floor (269U), 256U);
  TEST_COMPARE (stdc_bit_floor (511UL), 256UL);
  TEST_COMPARE (stdc_bit_floor (512UL), 512UL);
  TEST_COMPARE (stdc_bit_floor (513UL), 512ULL);
  TEST_COMPARE (stdc_bit_floor (1024ULL), 1024ULL);
  TEST_COMPARE (stdc_bit_ceil ((uc) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil ((uc) 0), uc), 1);
  TEST_COMPARE (stdc_bit_ceil ((us) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil ((us) 0), us), 1);
  TEST_COMPARE (stdc_bit_ceil (0U), 1U);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil (0U), ui), 1);
  TEST_COMPARE (stdc_bit_ceil (0UL), 1UL);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil (0UL), ul), 1);
  TEST_COMPARE (stdc_bit_ceil (0ULL), 1ULL);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil (0ULL), ull), 1);
  TEST_COMPARE (stdc_bit_ceil ((uc) ~0U), 0);
  TEST_COMPARE (stdc_bit_ceil ((us) ~0U), 0);
  TEST_COMPARE (stdc_bit_ceil (~0U), 0U);
  TEST_COMPARE (stdc_bit_ceil (~0UL), 0UL);
  TEST_COMPARE (stdc_bit_ceil (~0ULL), 0ULL);
  TEST_COMPARE (stdc_bit_ceil ((uc) ((uc) ~0U >> 1)), (1U << (CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil ((uc) ((uc) ~0U >> 1)), (1U << (CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil ((us) ((us) ~0U >> 1)),
		(1U << (sizeof (short) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil ((us) ((us) ~0U >> 1)),
		(1U << (sizeof (short) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil (~0U >> 1),
		(1U << (sizeof (int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil (1U << (sizeof (int) * CHAR_BIT - 1)),
		(1U << (sizeof (int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil (~0UL >> 1),
		(1UL << (sizeof (long int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil (~0UL >> 1),
		(1UL << (sizeof (long int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil (1ULL
			       << (sizeof (long long int) * CHAR_BIT - 1)),
		(1ULL << (sizeof (long long int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil (~0ULL >> 1),
		(1ULL << (sizeof (long long int) * CHAR_BIT - 1)));
  TEST_COMPARE (stdc_bit_ceil ((uc) 1), 1);
  TEST_COMPARE (stdc_bit_ceil ((uc) 2), 2);
  TEST_COMPARE (stdc_bit_ceil ((us) 3U), 4);
  TEST_COMPARE (stdc_bit_ceil ((us) 4U), 4);
  TEST_COMPARE (stdc_bit_ceil (5U), 8U);
  TEST_COMPARE (stdc_bit_ceil (269U), 512U);
  TEST_COMPARE (stdc_bit_ceil (511UL), 512UL);
  TEST_COMPARE (stdc_bit_ceil (512UL), 512UL);
  TEST_COMPARE (stdc_bit_ceil (513ULL), 1024ULL);
  TEST_COMPARE (stdc_bit_ceil (1025ULL), 2048ULL);
# ifdef __SIZEOF_INT128__
  TEST_COMPARE (stdc_leading_zeros ((unsigned __int128) 0),
		sizeof (__int128) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros ((unsigned __int128) 0), ui),
		1);
  TEST_COMPARE (stdc_leading_zeros (~(unsigned __int128) 0), 0);
  TEST_COMPARE (stdc_leading_ones ((unsigned __int128) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones ((unsigned __int128) 0), ui),
		1);
  TEST_COMPARE (stdc_leading_ones (~(unsigned __int128) 0),
		sizeof (__int128) * CHAR_BIT);
  TEST_COMPARE (stdc_trailing_zeros ((unsigned __int128) 0),
		sizeof (__int128) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros ((unsigned __int128) 0),
			       ui), 1);
  TEST_COMPARE (stdc_trailing_zeros (~(unsigned __int128) 0), 0);
  TEST_COMPARE (stdc_trailing_ones ((unsigned __int128) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones ((unsigned __int128) 0), ui),
		1);
  TEST_COMPARE (stdc_trailing_ones (~(unsigned __int128) 0),
		sizeof (__int128) * CHAR_BIT);
  TEST_COMPARE (stdc_first_leading_zero ((unsigned __int128) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero ((unsigned __int128) 0),
			       ui), 1);
  TEST_COMPARE (stdc_first_leading_zero (~(unsigned __int128) 0), 0);
  TEST_COMPARE (stdc_first_leading_one ((unsigned __int128) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one ((unsigned __int128) 0),
			       ui), 1);
  TEST_COMPARE (stdc_first_leading_one (~(unsigned __int128) 0), 1);
  TEST_COMPARE (stdc_first_trailing_zero ((unsigned __int128) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero ((unsigned __int128)
							 0), ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero (~(unsigned __int128) 0), 0);
  TEST_COMPARE (stdc_first_trailing_one ((unsigned __int128) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one ((unsigned __int128) 0),
			       ui), 1);
  TEST_COMPARE (stdc_first_trailing_one (~(unsigned __int128) 0), 1);
  TEST_COMPARE (stdc_count_zeros ((unsigned __int128) 0),
		sizeof (__int128) * CHAR_BIT);
  TEST_COMPARE (expr_has_type (stdc_count_zeros ((unsigned __int128) 0), ui),
		1);
  TEST_COMPARE (stdc_count_zeros (~(unsigned __int128) 0), 0);
  TEST_COMPARE (stdc_count_ones ((unsigned __int128) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones ((unsigned __int128) 0), ui),
		1);
  TEST_COMPARE (stdc_count_ones (~(unsigned __int128) 0),
		sizeof (__int128) * CHAR_BIT);
  TEST_COMPARE (stdc_has_single_bit ((unsigned __int128) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit ((unsigned __int128) 0),
		_Bool), 1);
  TEST_COMPARE (stdc_has_single_bit (~(unsigned __int128) 0), 0);
  TEST_COMPARE (stdc_bit_width ((unsigned __int128) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width ((unsigned __int128) 0), ui), 1);
  TEST_COMPARE (stdc_bit_width (~(unsigned __int128) 0),
		sizeof (__int128) * CHAR_BIT);
  TEST_COMPARE (stdc_bit_floor ((unsigned __int128) 0) != 0, 0);
  TEST_COMPARE (expr_has_type (stdc_bit_floor ((unsigned __int128) 0),
			       unsigned __int128), 1);
  TEST_COMPARE (stdc_bit_floor (~(unsigned __int128) 0)
		!= ((unsigned __int128) 1) << (sizeof (__int128)
					       * CHAR_BIT - 1), 0);
  TEST_COMPARE (stdc_bit_ceil ((unsigned __int128) 0) != 1, 0);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil ((unsigned __int128) 0),
			       unsigned __int128), 1);
  TEST_COMPARE (stdc_bit_ceil ((unsigned __int128) 1) != 1, 0);
  TEST_COMPARE (stdc_bit_ceil ((~(unsigned __int128) 0) >> 1)
		!= ((unsigned __int128) 1) << (sizeof (__int128)
					       * CHAR_BIT - 1), 0);
  TEST_COMPARE (stdc_bit_ceil (~(unsigned __int128) 0) != 0, 0);
# endif
  uc a = 0;
  TEST_COMPARE (stdc_bit_width (a++), 0);
  TEST_COMPARE (a, 1);
  ull b = 0;
  TEST_COMPARE (stdc_bit_width (b++), 0);
  TEST_COMPARE (b, 1);
  TEST_COMPARE (stdc_bit_floor (a++), 1);
  TEST_COMPARE (a, 2);
  TEST_COMPARE (stdc_bit_floor (b++), 1);
  TEST_COMPARE (b, 2);
  TEST_COMPARE (stdc_bit_ceil (a++), 2);
  TEST_COMPARE (a, 3);
  TEST_COMPARE (stdc_bit_ceil (b++), 2);
  TEST_COMPARE (b, 3);
  TEST_COMPARE (stdc_leading_zeros (a++), CHAR_BIT - 2);
  TEST_COMPARE (a, 4);
  TEST_COMPARE (stdc_leading_zeros (b++),
		sizeof (long long int) * CHAR_BIT - 2);
  TEST_COMPARE (b, 4);
  TEST_COMPARE (stdc_leading_ones (a++), 0);
  TEST_COMPARE (a, 5);
  TEST_COMPARE (stdc_leading_ones (b++), 0);
  TEST_COMPARE (b, 5);
  TEST_COMPARE (stdc_trailing_zeros (a++), 0);
  TEST_COMPARE (a, 6);
  TEST_COMPARE (stdc_trailing_zeros (b++), 0);
  TEST_COMPARE (b, 6);
  TEST_COMPARE (stdc_trailing_ones (a++), 0);
  TEST_COMPARE (a, 7);
  TEST_COMPARE (stdc_trailing_ones (b++), 0);
  TEST_COMPARE (b, 7);
  TEST_COMPARE (stdc_first_leading_zero (a++), 1);
  TEST_COMPARE (a, 8);
  TEST_COMPARE (stdc_first_leading_zero (b++), 1);
  TEST_COMPARE (b, 8);
  TEST_COMPARE (stdc_first_leading_one (a++), CHAR_BIT - 3);
  TEST_COMPARE (a, 9);
  TEST_COMPARE (stdc_first_leading_one (b++),
		sizeof (long long int) * CHAR_BIT - 3);
  TEST_COMPARE (b, 9);
  TEST_COMPARE (stdc_first_trailing_zero (a++), 2);
  TEST_COMPARE (a, 10);
  TEST_COMPARE (stdc_first_trailing_zero (b++), 2);
  TEST_COMPARE (b, 10);
  TEST_COMPARE (stdc_first_trailing_one (a++), 2);
  TEST_COMPARE (a, 11);
  TEST_COMPARE (stdc_first_trailing_one (b++), 2);
  TEST_COMPARE (b, 11);
  TEST_COMPARE (stdc_count_zeros (a++), CHAR_BIT - 3);
  TEST_COMPARE (a, 12);
  TEST_COMPARE (stdc_count_zeros (b++),
		sizeof (long long int) * CHAR_BIT - 3);
  TEST_COMPARE (b, 12);
  TEST_COMPARE (stdc_count_ones (a++), 2);
  TEST_COMPARE (a, 13);
  TEST_COMPARE (stdc_count_ones (b++), 2);
  TEST_COMPARE (b, 13);
  TEST_COMPARE (stdc_has_single_bit (a++), 0);
  TEST_COMPARE (a, 14);
  TEST_COMPARE (stdc_has_single_bit (b++), 0);
  TEST_COMPARE (b, 14);
# ifdef BITINT_MAXWIDTH
#  if BITINT_MAXWIDTH >= 64
  TEST_COMPARE (stdc_leading_zeros (0uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros (0uwb), ui), 1);
  TEST_COMPARE (stdc_leading_zeros (1uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros (1uwb), ui), 1);
  TEST_COMPARE (stdc_leading_ones (0uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones (0uwb), ui), 1);
  TEST_COMPARE (stdc_leading_ones (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_leading_ones (1uwb), ui), 1);
  TEST_COMPARE (stdc_trailing_zeros (0uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros (0uwb), ui), 1);
  TEST_COMPARE (stdc_trailing_zeros (1uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros (1uwb), ui), 1);
  TEST_COMPARE (stdc_trailing_ones (0uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones (0uwb), ui), 1);
  TEST_COMPARE (stdc_trailing_ones (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones (1uwb), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero (0uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero (0uwb), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero (1uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero (1uwb), ui), 1);
  TEST_COMPARE (stdc_first_leading_one (0uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one (0uwb), ui), 1);
  TEST_COMPARE (stdc_first_leading_one (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one (1uwb), ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero (0uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero (0uwb), ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero (1uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero (1uwb), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one (0uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one (0uwb), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one (1uwb), ui), 1);
  TEST_COMPARE (stdc_count_zeros (0uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_count_zeros (0uwb), ui), 1);
  TEST_COMPARE (stdc_count_zeros (1uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_count_zeros (1uwb), ui), 1);
  TEST_COMPARE (stdc_count_ones (0uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones (0uwb), ui), 1);
  TEST_COMPARE (stdc_count_ones (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_count_ones (1uwb), ui), 1);
  TEST_COMPARE (stdc_has_single_bit (0uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit (0uwb), _Bool), 1);
  TEST_COMPARE (stdc_has_single_bit (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit (1uwb), _Bool), 1);
  TEST_COMPARE (stdc_bit_width (0uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width (0uwb), ui), 1);
  TEST_COMPARE (stdc_bit_width (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_bit_width (1uwb), ui), 1);
  TEST_COMPARE (stdc_bit_floor (0uwb), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_floor (0uwb), unsigned _BitInt(1)), 1);
  TEST_COMPARE (stdc_bit_floor (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_bit_floor (1uwb), unsigned _BitInt(1)), 1);
  TEST_COMPARE (stdc_bit_ceil (0uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil (0uwb), unsigned _BitInt(1)), 1);
  TEST_COMPARE (stdc_bit_ceil (1uwb), 1);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil (1uwb), unsigned _BitInt(1)), 1);
  unsigned _BitInt(1) c = 0;
  TEST_COMPARE (stdc_bit_floor (c++), 0);
  TEST_COMPARE (c, 1);
  TEST_COMPARE (stdc_bit_floor (c++), 1);
  TEST_COMPARE (c, 0);
  TEST_COMPARE (stdc_bit_ceil (c++), 1);
  TEST_COMPARE (c, 1);
  TEST_COMPARE (stdc_bit_ceil (c++), 1);
  TEST_COMPARE (c, 0);
#  endif
#  if BITINT_MAXWIDTH >= 512
  TEST_COMPARE (stdc_leading_zeros ((unsigned _BitInt(512)) 0), 512);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros ((unsigned _BitInt(512)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_leading_zeros ((unsigned _BitInt(373)) 0), 373);
  TEST_COMPARE (expr_has_type (stdc_leading_zeros ((unsigned _BitInt(373)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_leading_zeros (~(unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (stdc_leading_zeros (~(unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (stdc_leading_zeros ((unsigned _BitInt(512)) 275), 512 - 9);
  TEST_COMPARE (stdc_leading_zeros ((unsigned _BitInt(373)) 512), 373 - 10);
  TEST_COMPARE (stdc_leading_ones ((unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones ((unsigned _BitInt(512)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_leading_ones ((unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_leading_ones ((unsigned _BitInt(373)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_leading_ones (~(unsigned _BitInt(512)) 0), 512);
  TEST_COMPARE (stdc_leading_ones (~(unsigned _BitInt(373)) 0), 373);
  TEST_COMPARE (stdc_leading_ones (~(unsigned _BitInt(512)) 275), 512 - 9);
  TEST_COMPARE (stdc_leading_ones (~(unsigned _BitInt(373)) 512), 373 - 10);
  TEST_COMPARE (stdc_trailing_zeros ((unsigned _BitInt(512)) 0), 512);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros ((unsigned _BitInt(512)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_trailing_zeros ((unsigned _BitInt(373)) 0), 373);
  TEST_COMPARE (expr_has_type (stdc_trailing_zeros ((unsigned _BitInt(373)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_trailing_zeros (~(unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (stdc_trailing_zeros (~(unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (stdc_trailing_zeros ((unsigned _BitInt(512)) 256), 8);
  TEST_COMPARE (stdc_trailing_zeros ((unsigned _BitInt(373)) 512), 9);
  TEST_COMPARE (stdc_trailing_ones ((unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones ((unsigned _BitInt(512)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_trailing_ones ((unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_trailing_ones ((unsigned _BitInt(373)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_trailing_ones (~(unsigned _BitInt(512)) 0), 512);
  TEST_COMPARE (stdc_trailing_ones (~(unsigned _BitInt(373)) 0), 373);
  TEST_COMPARE (stdc_trailing_ones ((unsigned _BitInt(512)) 255), 8);
  TEST_COMPARE (stdc_trailing_ones ((~(unsigned _BitInt(373)) 0) >> 2),
		373 - 2);
  TEST_COMPARE (stdc_first_leading_zero ((unsigned _BitInt(512)) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero ((unsigned _BitInt(512))
							0), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero ((unsigned _BitInt(373)) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_leading_zero ((unsigned _BitInt(373))
							0), ui), 1);
  TEST_COMPARE (stdc_first_leading_zero (~(unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (stdc_first_leading_zero (~(unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (stdc_first_leading_zero (~(unsigned _BitInt(512)) 511),
		512 - 8);
  TEST_COMPARE (stdc_first_leading_zero (~(unsigned _BitInt(373)) 1023),
		373 - 9);
  TEST_COMPARE (stdc_first_leading_one ((unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one ((unsigned _BitInt(512))
						       0), ui), 1);
  TEST_COMPARE (stdc_first_leading_one ((unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_leading_one ((unsigned _BitInt(373))
						       0), ui), 1);
  TEST_COMPARE (stdc_first_leading_one (~(unsigned _BitInt(512)) 0), 1);
  TEST_COMPARE (stdc_first_leading_one (~(unsigned _BitInt(373)) 0), 1);
  TEST_COMPARE (stdc_first_leading_one ((unsigned _BitInt(512)) 275), 512 - 8);
  TEST_COMPARE (stdc_first_leading_one ((unsigned _BitInt(373)) 512), 373 - 9);
  TEST_COMPARE (stdc_first_trailing_zero ((unsigned _BitInt(512)) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero ((unsigned
							  _BitInt(512)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero ((unsigned _BitInt(373)) 0), 1);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_zero ((unsigned
							  _BitInt(373)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_first_trailing_zero (~(unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (stdc_first_trailing_zero (~(unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (stdc_first_trailing_zero ((unsigned _BitInt(512)) 255), 9);
  TEST_COMPARE (stdc_first_trailing_zero ((unsigned _BitInt(373)) 511), 10);
  TEST_COMPARE (stdc_first_trailing_one ((unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one ((unsigned _BitInt(512))
							0), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one ((unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_first_trailing_one ((unsigned _BitInt(373))
							0), ui), 1);
  TEST_COMPARE (stdc_first_trailing_one (~(unsigned _BitInt(512)) 0), 1);
  TEST_COMPARE (stdc_first_trailing_one (~(unsigned _BitInt(373)) 0), 1);
  TEST_COMPARE (stdc_first_trailing_one (((unsigned _BitInt(512)) 255) << 175),
		176);
  TEST_COMPARE (stdc_first_trailing_one ((~(unsigned _BitInt(373)) 0) << 311),
		312);
  TEST_COMPARE (stdc_count_zeros ((unsigned _BitInt(512)) 0), 512);
  TEST_COMPARE (expr_has_type (stdc_count_zeros ((unsigned _BitInt(512)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_count_zeros ((unsigned _BitInt(373)) 0), 373);
  TEST_COMPARE (expr_has_type (stdc_count_zeros ((unsigned _BitInt(373)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_count_zeros (~(unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (stdc_count_zeros (~(unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (stdc_count_zeros ((unsigned _BitInt(512)) 1315), 512 - 5);
  TEST_COMPARE (stdc_count_zeros ((unsigned _BitInt(373)) 3363), 373 - 6);
  TEST_COMPARE (stdc_count_ones ((unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones ((unsigned _BitInt(512)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_count_ones ((unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_count_ones ((unsigned _BitInt(373)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_count_ones (~(unsigned _BitInt(512)) 0), 512);
  TEST_COMPARE (stdc_count_ones (~(unsigned _BitInt(373)) 0), 373);
  TEST_COMPARE (stdc_count_ones (~(unsigned _BitInt(512)) 1315), 512 - 5);
  TEST_COMPARE (stdc_count_ones (~(unsigned _BitInt(373)) 3363), 373 - 6);
  TEST_COMPARE (stdc_has_single_bit ((unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit ((unsigned _BitInt(512)) 0),
			       _Bool), 1);
  TEST_COMPARE (stdc_has_single_bit ((unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_has_single_bit ((unsigned _BitInt(373)) 0),
			       _Bool), 1);
  TEST_COMPARE (stdc_has_single_bit (~(unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (stdc_has_single_bit (~(unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (stdc_has_single_bit (((unsigned _BitInt(512)) 1022) << 279),
		0);
  TEST_COMPARE (stdc_has_single_bit (((unsigned _BitInt(373)) 12) << 305), 0);
  TEST_COMPARE (stdc_bit_width ((unsigned _BitInt(512)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width ((unsigned _BitInt(512)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_bit_width ((unsigned _BitInt(373)) 0), 0);
  TEST_COMPARE (expr_has_type (stdc_bit_width ((unsigned _BitInt(373)) 0),
			       ui), 1);
  TEST_COMPARE (stdc_bit_width (~(unsigned _BitInt(512)) 0), 512);
  TEST_COMPARE (stdc_bit_width (~(unsigned _BitInt(373)) 0), 373);
  TEST_COMPARE (stdc_bit_width (((unsigned _BitInt(512)) 1023) << 405),
		405 + 10);
  TEST_COMPARE (stdc_bit_width (((unsigned _BitInt(373)) 1024) << 242),
		242 + 11);
  TEST_COMPARE (stdc_bit_floor ((unsigned _BitInt(512)) 0) != 0, 0);
  TEST_COMPARE (expr_has_type (stdc_bit_floor ((unsigned _BitInt(512)) 0),
			       unsigned _BitInt(512)), 1);
  TEST_COMPARE (stdc_bit_floor ((unsigned _BitInt(373)) 0) != 0, 0);
  TEST_COMPARE (expr_has_type (stdc_bit_floor ((unsigned _BitInt(373)) 0),
			       unsigned _BitInt(373)), 1);
  TEST_COMPARE (stdc_bit_floor (~(unsigned _BitInt(512)) 0)
		!= ((unsigned _BitInt(512)) 1) << (512 - 1), 0);
  TEST_COMPARE (stdc_bit_floor (~(unsigned _BitInt(373)) 0)
		!= ((unsigned _BitInt(373)) 1) << (373 - 1), 0);
  TEST_COMPARE (stdc_bit_floor (((unsigned _BitInt(512)) 511) << 405)
		!= (((unsigned _BitInt(512)) 256) << 405), 0);
  TEST_COMPARE (stdc_bit_floor (((unsigned _BitInt(373)) 512) << 242)
		!= (((unsigned _BitInt(512)) 512) << 242), 0);
  TEST_COMPARE (stdc_bit_ceil ((unsigned _BitInt(512)) 0) != 1, 0);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil ((unsigned _BitInt(512)) 0),
			       unsigned _BitInt(512)), 1);
  TEST_COMPARE (stdc_bit_ceil ((unsigned _BitInt(373)) 0) != 1, 0);
  TEST_COMPARE (expr_has_type (stdc_bit_ceil ((unsigned _BitInt(373)) 0),
			       unsigned _BitInt(373)), 1);
  TEST_COMPARE (stdc_bit_ceil (~(unsigned _BitInt(512)) 0) != 0, 0);
  TEST_COMPARE (stdc_bit_ceil (~(unsigned _BitInt(373)) 0) != 0, 0);
  TEST_COMPARE (stdc_bit_ceil (((unsigned _BitInt(512)) 1) << (512 - 1))
		!= ((unsigned _BitInt(512)) 1) << (512 - 1), 0);
  TEST_COMPARE (stdc_bit_ceil ((~(unsigned _BitInt(373)) 0) >> 1)
		!= ((unsigned _BitInt(373)) 1) << (373 - 1), 0);
  TEST_COMPARE (stdc_bit_ceil (((unsigned _BitInt(512)) 512) << 405)
		!= (((unsigned _BitInt(512)) 512) << 405), 0);
  TEST_COMPARE (stdc_bit_ceil (((unsigned _BitInt(373)) 513) << 242)
		!= (((unsigned _BitInt(512)) 1024) << 242), 0);
  TEST_COMPARE (stdc_bit_floor ((unsigned _BitInt(BITINT_MAXWIDTH)) 0) != 0,
		0);
  TEST_COMPARE (stdc_bit_floor (~(unsigned _BitInt(BITINT_MAXWIDTH)) 0)
		!= ((unsigned _BitInt(BITINT_MAXWIDTH)) 1) << (BITINT_MAXWIDTH
							       - 1), 0);
  TEST_COMPARE (stdc_bit_floor (((unsigned _BitInt(BITINT_MAXWIDTH)) 511)
				<< 405)
		!= (((unsigned _BitInt(BITINT_MAXWIDTH)) 256) << 405), 0);
  TEST_COMPARE (stdc_bit_floor (((unsigned _BitInt(BITINT_MAXWIDTH)) 512)
				<< 405)
		!= (((unsigned _BitInt(BITINT_MAXWIDTH)) 512) << 405), 0);
  TEST_COMPARE (stdc_bit_ceil ((unsigned _BitInt(BITINT_MAXWIDTH)) 0) != 1, 0);
  TEST_COMPARE (stdc_bit_ceil (~(unsigned _BitInt(BITINT_MAXWIDTH)) 0) != 0,
		0);
  TEST_COMPARE (stdc_bit_ceil (((unsigned _BitInt(BITINT_MAXWIDTH)) 1)
			       << (BITINT_MAXWIDTH - 1))
		!= ((unsigned _BitInt(BITINT_MAXWIDTH)) 1) << (BITINT_MAXWIDTH
							       - 1), 0);
  TEST_COMPARE (stdc_bit_ceil (((unsigned _BitInt(BITINT_MAXWIDTH)) 512)
			       << 405)
		!= (((unsigned _BitInt(BITINT_MAXWIDTH)) 512) << 405), 0);
  TEST_COMPARE (stdc_bit_ceil (((unsigned _BitInt(BITINT_MAXWIDTH)) 513)
			       << 405)
		!= (((unsigned _BitInt(BITINT_MAXWIDTH)) 1024) << 405), 0);
#  endif
# endif
  return 0;
}
#else
static int
do_test (void)
{
  return 0;
}
#endif

#include <support/test-driver.c>
