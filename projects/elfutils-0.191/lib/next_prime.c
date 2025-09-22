/* Determine prime number.
   Copyright (C) 2006 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2000.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#include <stddef.h>


/* Test whether CANDIDATE is a prime.  */
static int
is_prime (size_t candidate)
{
  /* No even number and none less than 10 will be passed here.  */
  size_t divn = 3;
  size_t sq = divn * divn;

  while (sq < candidate && candidate % divn != 0)
    {
      size_t old_sq = sq;
      ++divn;
      sq += 4 * divn;
      if (sq < old_sq)
	return 1;
      ++divn;
    }

  return candidate % divn != 0;
}


/* We need primes for the table size.  */
size_t
next_prime (size_t seed)
{
  /* Make it definitely odd.  */
  seed |= 1;

  while (!is_prime (seed))
    seed += 2;

  return seed;
}
