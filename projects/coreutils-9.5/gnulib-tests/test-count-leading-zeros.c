/*
 * Copyright (C) 2012-2024 Free Software Foundation, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Eric Blake.  */

#include <config.h>

#include "count-leading-zeros.h"

#include <limits.h>
#include <stdio.h>

#include "macros.h"

#define UINT_BIT (sizeof (unsigned int) * CHAR_BIT)
#define ULONG_BIT (sizeof (unsigned long int) * CHAR_BIT)
#define ULLONG_BIT (sizeof (unsigned long long int) * CHAR_BIT)

int
main (int argc, char *argv[])
{
  int i, j;

#define TEST_COUNT_LEADING_ZEROS(FUNC, TYPE, BITS, MAX, ONE)     \
  ASSERT (FUNC (0) == BITS);                                     \
  for (i = 0; i < BITS; i++)                                     \
    {                                                            \
      ASSERT (FUNC (ONE << i) == BITS - i - 1);                  \
      for (j = 0; j < i; j++)                                    \
        ASSERT (FUNC ((ONE << i) | (ONE << j)) == BITS - i - 1); \
    }                                                            \
  for (i = 0; i < 1000; i++)                                     \
    {                                                            \
      /* RAND_MAX is most often 0x7fff or 0x7fffffff.  */        \
      TYPE value =                                               \
        (RAND_MAX <= 0xffff                                      \
         ? ((TYPE) rand () >> 3)                                 \
           ^ (((TYPE) rand () >> 3) << 12)                       \
           ^ (((TYPE) rand () >> 3) << 24)                       \
           ^ (((TYPE) rand () >> 3) << 30 << 6)                  \
           ^ (((TYPE) rand () >> 3) << 30 << 18)                 \
           ^ (((TYPE) rand () >> 3) << 30 << 30)                 \
         : ((TYPE) rand () >> 3)                                 \
           ^ (((TYPE) rand () >> 3) << 22)                       \
           ^ (((TYPE) rand () >> 3) << 22 << 22));               \
      int count = 0;                                             \
      for (j = 0; j < BITS; j++)                                 \
        if (value & (ONE << j))                                  \
          count = BITS - j - 1;                                  \
      ASSERT (count == FUNC (value));                            \
    }                                                            \
  ASSERT (FUNC (MAX) == 0);

  TEST_COUNT_LEADING_ZEROS (count_leading_zeros, unsigned int,
                            UINT_BIT, UINT_MAX, 1U);
  TEST_COUNT_LEADING_ZEROS (count_leading_zeros_l, unsigned long int,
                            ULONG_BIT, ULONG_MAX, 1UL);
  TEST_COUNT_LEADING_ZEROS (count_leading_zeros_ll, unsigned long long int,
                            ULLONG_BIT, ULLONG_MAX, 1ULL);

  return 0;
}
