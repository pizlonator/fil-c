/* Clear a block of memory for calloc.  Generic version.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <string.h>

static __always_inline void *
clear_memory (void *p, unsigned long clearsize)
{
  INTERNAL_SIZE_T *d = (INTERNAL_SIZE_T *) p;
  /* Unroll clear memory size up to 9 * INTERNAL_SIZE_T bytes.  We know
     that contents have an odd number of INTERNAL_SIZE_T-sized words;
     minimally 3 words.  */
  unsigned long nclears = clearsize / SIZE_SZ;

  if (nclears > 9)
    return memset (d, 0, clearsize);

  /* NB: The VRP pass in GCC 14.2 will optimize it out.  */
  if (nclears < 3)
    __builtin_unreachable ();

  /* Use repeated stores with 1 branch, instead of up to 3.  */
  *(d + 0) = 0;
  *(d + 1) = 0;
  *(d + 2) = 0;
  *(d + nclears - 2) = 0;
  *(d + nclears - 2 + 1) = 0;
  if (nclears > 5)
    {
      *(d + 3) = 0;
      *(d + 3 + 1) = 0;
      *(d + nclears - 4) = 0;
      *(d + nclears - 4 + 1) = 0;
    }

  return d;
}
