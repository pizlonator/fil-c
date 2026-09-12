/* Finding the next prime >= a given small integer.
   Copyright (C) 1995-2026 Free Software Foundation, Inc.

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

#include <config.h>
#ifdef _LIBC
# include <stdbool.h>
#endif

/* Specification.  */
#include "next-prime.h"

#include <stdint.h> /* for SIZE_MAX */

/* Return true if CANDIDATE is a prime number or 1.
   CANDIDATE should be an odd number >= 1.  */
static bool _GL_ATTRIBUTE_CONST
is_prime (size_t candidate)
{
  size_t divisor = 3;
  size_t square = divisor * divisor;

  for (;;)
    {
      if (square > candidate)
        return true;
      if ((candidate % divisor) == 0)
        return false;
      /* Increment divisor by 2.  */
      divisor++;
      square += 4 * divisor;
      divisor++;
    }
}

size_t _GL_ATTRIBUTE_CONST
next_prime (size_t candidate)
{
#if !defined IN_LIBGETTEXTLIB
  /* Skip small primes.  */
  if (candidate < 10)
    candidate = 10;
#endif

  /* Make it definitely odd.  */
  candidate |= 1;

  while (SIZE_MAX != candidate && !is_prime (candidate))
    candidate += 2;

  return candidate;
}
