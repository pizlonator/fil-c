/* Random pseudo generator number which returns a single 32 bit value
   uniformly distributed but with an upper_bound.
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

#include <stdlib.h>
#include <sys/param.h>

/* Return a uniformly distributed random number less than N.  The algorithm
   calculates a mask being the lowest power of two bounding the upper bound
   N, successively queries new random values, and rejects values outside of
   the request range.

   For reject values, it also tries if the remaining entropy could fit on
   the asked range after range adjustment.

   The algorithm avoids modulo and divide operations, which might be costly
   depending on the architecture.  */
uint32_t
__arc4random_uniform (uint32_t n)
{
  if (n <= 1)
    /* There is no valid return value for a zero limit, and 0 is the
       only possible result for limit 1.  */
    return 0;

  /* Powers of two are easy.  */
  if (powerof2 (n))
    return __arc4random () & (n - 1);

  /* mask is the smallest power of 2 minus 1 number larger than n.  */
  int z = __builtin_clz (n);
  uint32_t mask = ~UINT32_C(0) >> z;
  int bits = CHAR_BIT * sizeof (uint32_t) - z;

  while (1)
    {
      uint32_t value = __arc4random ();

      /* Return if the lower power of 2 minus 1 satisfy the condition.  */
      uint32_t r = value & mask;
      if (r < n)
	return r;

      /* Otherwise check if remaining bits of entropy provides fits in the
	 bound.  */
      for (int bits_left = z; bits_left >= bits; bits_left -= bits)
	{
	  value >>= bits;
	  r = value & mask;
	  if (r < n)
	    return r;
	}
    }
}
libc_hidden_def (__arc4random_uniform)
weak_alias (__arc4random_uniform, arc4random_uniform)
