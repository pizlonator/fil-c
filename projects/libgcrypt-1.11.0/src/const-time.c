/* const-time.c  -  Constant-time functions
 *      Copyright (C) 2023  g10 Code GmbH
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include "g10lib.h"
#include "const-time.h"


#ifndef HAVE_GCC_ASM_VOLATILE_MEMORY
/* These variables are used to generate masks from conditional operation
 * flag parameters.  Use of volatile prevents compiler optimizations from
 * converting AND-masking to conditional branches.  */
volatile unsigned int _gcry_ct_vzero = 0;
volatile unsigned int _gcry_ct_vone = 1;
#endif


/*
 * Compare byte arrays of length LEN, return 1 if it's not same,
 * 0, otherwise.
 */
unsigned int
_gcry_ct_not_memequal (const void *b1, const void *b2, size_t len)
{
  const byte *a = b1;
  const byte *b = b2;
  int ab, ba;
  size_t i;

  /* Constant-time compare. */
  for (i = 0, ab = 0, ba = 0; i < len; i++)
    {
      /* If a[i] != b[i], either ab or ba will be negative. */
      ab |= a[i] - b[i];
      ba |= b[i] - a[i];
    }

  /* 'ab | ba' is negative when buffers are not equal, extract sign bit.  */
  return ((unsigned int)(ab | ba) >> (sizeof(unsigned int) * 8 - 1)) & 1;
}

/*
 * Compare byte arrays of length LEN, return 0 if it's not same,
 * 1, otherwise.
 */
unsigned int
_gcry_ct_memequal (const void *b1, const void *b2, size_t len)
{
  return _gcry_ct_not_memequal (b1, b2, len) ^ 1;
}

/*
 * Copy LEN bytes from memory area SRC to memory area DST, when
 * OP_ENABLED=1.  When DST <= SRC, the memory areas may overlap.  When
 * DST > SRC, the memory areas must not overlap.
 */
void
_gcry_ct_memmov_cond (void *dst, const void *src, size_t len,
		      unsigned long op_enable)
{
  /* Note: dual mask with AND/OR used for EM leakage mitigation */
  unsigned char mask1 = ct_ulong_gen_mask(op_enable);
  unsigned char mask2 = ct_ulong_gen_inv_mask(op_enable);
  unsigned char *b_dst = dst;
  const unsigned char *b_src = src;
  size_t i;

  for (i = 0; i < len; i++)
    b_dst[i] = (b_dst[i] & mask2) | (b_src[i] & mask1);
}
