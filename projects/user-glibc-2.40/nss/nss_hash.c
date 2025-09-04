/* Copyright (c) 1997-2024 Free Software Foundation, Inc.
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

#include <nss.h>

/* This is from libc/db/hash/hash_func.c, hash3 is static there */
/*
 * This is INCREDIBLY ugly, but fast.  We break the string up into 4 byte
 * units.  On the first time through the loop we get the "leftover bytes"
 * (len % 4).  On every other iteration, we perform a 4x unrolled version
 * HASHC. Further unrolling does not appear to help.
 *
 * OZ's original sdbm hash
 */
uint32_t
__nss_hash (const void *keyarg, size_t len)
{
  enum
  {
    HASH_CONST_P0 = 1,	       /* (uint32_t)(65599 ^ 0).  */
    HASH_CONST_P1 = 65599,     /* (uint32_t)(65599 ^ 1).  */
    HASH_CONST_P2 = 8261505,   /* (uint32_t)(65599 ^ 2).  */
    HASH_CONST_P3 = 780587199, /* (uint32_t)(65599 ^ 3).  */
    HASH_CONST_P4 = 1139564289 /* (uint32_t)(65599 ^ 4).  */
  };

  const unsigned char *key;
  uint32_t h;

#define HASHC	h = *key++ + HASH_CONST_P1 * h

  h = 0;
  key = keyarg;
  if (len > 0)
    {
      switch ((len & (4 - 1)))
	{
	case 0:
	  /* h starts out as zero so no need to include the multiply. */
	  h = *key++;
	  /* FALLTHROUGH */
	case 3:
	  HASHC;
	  /* FALLTHROUGH */
	case 2:
	  HASHC;
	  /* FALLTHROUGH */
	case 1:
	  HASHC;
	  /* FALLTHROUGH */
	}

      uint32_t c0, c1, c2, c3;
      for (--len; len >= 4; len -= 4)
	{
	  c0 = (unsigned char) *(key + 0);
	  c1 = (unsigned char) *(key + 1);
	  c2 = (unsigned char) *(key + 2);
	  c3 = (unsigned char) *(key + 3);
	  h = HASH_CONST_P4 * h + HASH_CONST_P3 * c0 + HASH_CONST_P2 * c1
	      + HASH_CONST_P1 * c2 + HASH_CONST_P0 * c3;

	  key += 4;
	}
    }
  return h;
}


libc_hidden_def (__nss_hash)
