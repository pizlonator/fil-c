/* Conversion functions for versioning information.
   Copyright (C) 2006, 2007 Red Hat, Inc.
   Copyright (C) 2023, Mark J. Wielaard <mark@klomp.org>
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2006.

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

#include <assert.h>
#include <gelf.h>

#include "libelfP.h"


static void
elf_cvt_gnuhash (void *dest, const void *src, size_t len, int encode)
{
  size_t size = len;
  /* The GNU hash table format on 64 bit machines mixes 32 bit and 64 bit
     words.  We must detangle them here.   */
  Elf32_Word *dest32 = dest;
  const Elf32_Word *src32 = src;

  /* First four control words, 32 bits.  */
  for (unsigned int cnt = 0; cnt < 4; ++cnt)
    {
      if (len < 4)
	goto done;
      dest32[cnt] = bswap_32 (src32[cnt]);
      len -= 4;
    }

  Elf32_Word bitmask_words = encode ? src32[2] : dest32[2];

  /* Now the 64 bit words.  */
  Elf64_Xword *dest64 = (Elf64_Xword *) &dest32[4];
  const Elf64_Xword *src64 = (const Elf64_Xword *) &src32[4];
  for (unsigned int cnt = 0; cnt < bitmask_words; ++cnt)
    {
      if (len < 8)
	goto done;
      dest64[cnt] = bswap_64 (src64[cnt]);
      len -= 8;
    }

  /* The rest are 32 bit words again.  */
  src32 = (const Elf32_Word *) &src64[bitmask_words];
  dest32 = (Elf32_Word *) &dest64[bitmask_words];
  while (len >= 4)
    {
      *dest32++ = bswap_32 (*src32++);
      len -= 4;
    }

 done:
  /* If there are any bytes left, we weren't able to convert the
     partial structures, just copy them over. */
  if (len > 0)
    memmove (dest + size - len, src + size - len, len);
}
