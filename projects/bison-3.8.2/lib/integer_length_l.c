/* integer_length_l - find most significant bit in an 'unsigned long'.
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
#include <stddef.h>

#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#ifdef USE_LONG_LONG
# define FUNC integer_length_ll
# define TYPE unsigned long long
# define GCC_BUILTIN __builtin_clzll
# ifdef _WIN64
#  define MSVC_BUILTIN _BitScanReverse64
# endif
#else
# define FUNC integer_length_l
# define TYPE unsigned long
# define GCC_BUILTIN __builtin_clzl
# define MSVC_BUILTIN _BitScanReverse
#endif

#if defined _MSC_VER && !(__clang_major__ >= 4)
# include <intrin.h>
/* Copied from integer_length.c.  */
static inline int
integer_length (unsigned int x)
{
  /* _BitScanReverse
     <https://docs.microsoft.com/en-us/cpp/intrinsics/bitscanreverse-bitscanreverse64> */
  unsigned long bit;
  if (_BitScanReverse (&bit, x))
    return bit + 1;
  else
    return 0;
}
#endif

#define NBITS (sizeof (TYPE) * CHAR_BIT)

/* Modify shift count to be in bounds, even in dead code, otherwise GCC
   emits warnings "right shift count >= width of type".  */
#define MSC(i) MIN (i, NBITS - 1)

int
FUNC (TYPE x)
{
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || (__clang_major__ >= 4)
  if (x == 0)
    return 0;
  else
    return NBITS - GCC_BUILTIN (x);
#elif defined _MSC_VER && defined MSVC_BUILTIN
  /* _BitScanReverse, _BitScanReverse64
     <https://docs.microsoft.com/en-us/cpp/intrinsics/bitscanreverse-bitscanreverse64> */
  unsigned long bit;
  if (MSVC_BUILTIN (&bit, x))
    return bit + 1;
  else
    return 0;
#else
  /* Split x into chunks, and look at one chunk after the other.  */
  if (sizeof (TYPE) > 2 * sizeof (unsigned int))
    {
      /* Generic loop.  */
      size_t i;

      for (i = (sizeof (TYPE) - 1) / sizeof (unsigned int); i >= 2; i--)
        {
          unsigned int y = x >> (i * sizeof (unsigned int) * CHAR_BIT);
          if (y != 0)
            return i * sizeof (unsigned int) * CHAR_BIT + integer_length (y);
        }
    }

  /* Unrolled the last 2 loop rounds.  */
  /* i = 1 */
  if (sizeof (TYPE) > 1 * sizeof (unsigned int))
    {
      unsigned int y = x >> MSC (1 * sizeof (unsigned int) * CHAR_BIT);
      if (y != 0)
        return 1 * sizeof (unsigned int) * CHAR_BIT + integer_length (y);
    }

  /* i = 0 */
  return integer_length ((unsigned int) x);
#endif
}
