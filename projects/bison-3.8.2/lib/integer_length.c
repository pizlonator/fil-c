/* integer_length - find most significant bit in an 'unsigned int'.
   Copyright (C) 2011-2021 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2011.  */

#include <config.h>

/* Specification.  */
#include "integer_length.h"

#include <limits.h>

#include "float+.h"

#if defined _MSC_VER && !(__clang_major__ >= 4)
# include <intrin.h>
#endif

#define NBITS (sizeof (unsigned int) * CHAR_BIT)

int
integer_length (unsigned int x)
{
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__clang_major__ >= 4)
  if (x == 0)
    return 0;
  else
    return NBITS - __builtin_clz (x);
#elif defined _MSC_VER
  /* _BitScanReverse
     <https://docs.microsoft.com/en-us/cpp/intrinsics/bitscanreverse-bitscanreverse64> */
  unsigned long bit;
  if (_BitScanReverse (&bit, x))
    return bit + 1;
  else
    return 0;
#else
# if defined DBL_EXPBIT0_WORD && defined DBL_EXPBIT0_BIT
  if (NBITS <= DBL_MANT_BIT)
    {
      /* Use 'double' operations.
         Assumes an IEEE 754 'double' implementation.  */
#  define DBL_EXP_MASK ((DBL_MAX_EXP - DBL_MIN_EXP) | 7)
#  define DBL_EXP_BIAS (DBL_EXP_MASK / 2 - 1)
#  define NWORDS \
    ((sizeof (double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
      typedef union { double value; unsigned int word[NWORDS]; }
              memory_double;

      if (x == 0)
        return 0;
      else
        {
          memory_double m;
          unsigned int exponent;

          if (1)
            {
              /* Use a single integer to floating-point conversion.  */
              m.value = x;
            }
          else
            {
              /* Use a single floating-point subtraction.  */
              /* 2^(DBL_MANT_DIG-1).  */
              static const double TWO_DBL_MANT_DIG =
                /* Assume DBL_MANT_DIG <= 5 * 31.
                   Use the identity
                   n = floor(n/5) + floor((n+1)/5) + ... + floor((n+4)/5).  */
                (double) (1U << ((DBL_MANT_DIG - 1) / 5))
                * (double) (1U << ((DBL_MANT_DIG - 1 + 1) / 5))
                * (double) (1U << ((DBL_MANT_DIG - 1 + 2) / 5))
                * (double) (1U << ((DBL_MANT_DIG - 1 + 3) / 5))
                * (double) (1U << ((DBL_MANT_DIG - 1 + 4) / 5));

              /* Construct 2^(DBL_MANT_DIG-1) + x by hand.  */
              m.word[DBL_EXPBIT0_WORD] =
                (DBL_MANT_DIG + DBL_EXP_BIAS) << DBL_EXPBIT0_BIT;
              m.word[1 - DBL_EXPBIT0_WORD] = x;

              /* Subtract 2^(DBL_MANT_DIG-1).  */
              m.value = m.value - TWO_DBL_MANT_DIG;
            }

          exponent =
            (m.word[DBL_EXPBIT0_WORD] >> DBL_EXPBIT0_BIT) & DBL_EXP_MASK;
          return exponent - DBL_EXP_BIAS;
        }
    }
  else
# endif
    if (NBITS == 32)
      {
        /* 6 comparisons.  */
        if (x != 0)
          {
            int result = 1;
            if (x >= 0x10000)
              {
                x = x >> 16;
                result += 16;
              }
            if (x >= 0x100)
              {
                x = x >> 8;
                result += 8;
              }
            if (x >= 0x10)
              {
                x = x >> 4;
                result += 4;
              }
            if (x >= 0x4)
              {
                x = x >> 2;
                result += 2;
              }
            if (x >= 0x2)
              {
                x = x >> 1;
                result += 1;
              }
            return result;
          }
        else
          return 0;
      }
    else
      {
        /* Naive loop.
           Works for any value of NBITS.  */
        int j;

        for (j = NBITS - 1; j >= 0; j--)
          if (x & (1U << j))
            return j + 1;
        return 0;
      }
#endif
}
