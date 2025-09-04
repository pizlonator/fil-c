/* strspn with SSE4.2 intrinsics
   Copyright (C) 2009-2024 Free Software Foundation, Inc.
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

#include <nmmintrin.h>
#include <string.h>
#include "varshift.h"

/* We use 0x12:
	_SIDD_SBYTE_OPS
	| _SIDD_CMP_EQUAL_ANY
	| _SIDD_NEGATIVE_POLARITY
	| _SIDD_LEAST_SIGNIFICANT
   on pcmpistri to compare xmm/mem128

   0 1 2 3 4 5 6 7 8 9 A B C D E F
   X X X X X X X X X X X X X X X X

   against xmm

   0 1 2 3 4 5 6 7 8 9 A B C D E F
   A A A A A A A A A A A A A A A A

   to find out if the first 16byte data element has any non-A byte and
   the offset of the first byte.  There are 2 cases:

   1. The first 16byte data element has the non-A byte, including
      EOS, at the offset X.
   2. The first 16byte data element is valid and doesn't have the non-A
      byte.

   Here is the table of ECX, CFlag, ZFlag and SFlag for 2 cases:

   case		ECX	CFlag	ZFlag	SFlag
    1		 X	  1	 0/1	  0
    2		16	  0	  0	  0

   We exit from the loop for case 1.  */

extern size_t __strspn_generic (const char *, const char *) attribute_hidden;

#ifndef STRSPN
# define STRSPN	__strspn_sse42
#endif

size_t
__attribute__ ((section (".text.sse4.2")))
STRSPN (const char *s, const char *a)
{
  if (*a == 0)
    return 0;

  const char *aligned;
  __m128i mask, maskz, zero;
  unsigned int maskz_bits;
  unsigned int offset = (int) ((size_t) a & 15);
  zero = _mm_set1_epi8 (0);
  if (offset != 0)
    {
      /* Load masks.  */
      aligned = (const char *) ((size_t) a & -16L);
      __m128i mask0 = _mm_load_si128 ((__m128i *) aligned);
      maskz = _mm_cmpeq_epi8 (mask0, zero);

      /* Find where the NULL terminator is.  */
      maskz_bits = _mm_movemask_epi8 (maskz) >> offset;
      if (maskz_bits != 0)
        {
          mask = __m128i_shift_right (mask0, offset);
          offset = (unsigned int) ((size_t) s & 15);
          if (offset)
            goto start_unaligned;

          aligned = s;
          goto start_loop;
        }
    }

  /* A is aligned.  */
  mask = _mm_loadu_si128 ((__m128i *) a);

  /* Find where the NULL terminator is.  */
  maskz = _mm_cmpeq_epi8 (mask, zero);
  maskz_bits = _mm_movemask_epi8 (maskz);
  if (maskz_bits == 0)
    {
      /* There is no NULL terminator.  Don't use SSE4.2 if the length
         of A > 16.  */
      if (a[16] != 0)
        return __strspn_generic (s, a);
    }
  aligned = s;
  offset = (unsigned int) ((size_t) s & 15);

  if (offset != 0)
    {
    start_unaligned:
      /* Check partial string.  */
      aligned = (const char *) ((size_t) s & -16L);
      __m128i value = _mm_load_si128 ((__m128i *) aligned);
      __m128i adj_value = __m128i_shift_right (value, offset);

      unsigned int length = _mm_cmpistri (mask, adj_value, 0x12);
      /* No need to check CFlag since it is always 1.  */
      if (length < 16 - offset)
	return length;
      /* Find where the NULL terminator is.  */
      maskz = _mm_cmpeq_epi8 (value, zero);
      maskz_bits = _mm_movemask_epi8 (maskz) >> offset;
      if (maskz_bits != 0)
	return length;
      aligned += 16;
    }

start_loop:
  while (1)
    {
      __m128i value = _mm_load_si128 ((__m128i *) aligned);
      unsigned int index = _mm_cmpistri (mask, value, 0x12);
      unsigned int cflag = _mm_cmpistrc (mask, value, 0x12);
      if (cflag)
	return (size_t) (aligned + index - s);
      aligned += 16;
    }
}
