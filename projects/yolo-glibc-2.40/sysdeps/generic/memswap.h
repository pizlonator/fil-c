/* Swap the content of two memory blocks, overlap is NOT handled.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

static inline void
__memswap (void *__restrict p1, void *__restrict p2, size_t n)
{
  /* Use multiple small memcpys with constant size to enable inlining on most
     targets.  */
  enum { SWAP_GENERIC_SIZE = 32 };
  unsigned char tmp[SWAP_GENERIC_SIZE];
  while (n > SWAP_GENERIC_SIZE)
    {
      memcpy (tmp, p1, SWAP_GENERIC_SIZE);
      p1 = __mempcpy (p1, p2, SWAP_GENERIC_SIZE);
      p2 = __mempcpy (p2, tmp, SWAP_GENERIC_SIZE);
      n -= SWAP_GENERIC_SIZE;
    }
  while (n > 0)
    {
      unsigned char t = ((unsigned char *)p1)[--n];
      ((unsigned char *)p1)[n] = ((unsigned char *)p2)[n];
      ((unsigned char *)p2)[n] = t;
    }
}
