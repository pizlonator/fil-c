/* _dl_new_hash for elf symbol lookup
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

#ifndef _DL_NEW_HASH_H
#define _DL_NEW_HASH_H 1

#include <stdint.h>
/* For __always_inline and __glibc_unlikely.  */
#include <sys/cdefs.h>

/* The simplest implementation of _dl_new_hash is:

   _dl_new_hash (const char *s)
   {
      uint32_t h = 5381;
      for (unsigned char c = *s; c != '\0'; c = *++s)
        h = h * 33 + c;
      return h;
   }

   We can get better performance by slightly unrolling the loop to
   pipeline the multiples, which gcc cannot easily do due to
   dependencies across iterations.

   As well, as an architecture specific option we add asm statements
   to explicitly specify order of operations and prevent reassociation
   of instructions that lengthens the loop carried dependency. This
   may have no affect as the compiler may have ordered instructions
   the same way without it but in testing this has not been the case
   for GCC. Improving GCC to reliably schedule instructions ideally
   cannot be easily done.

   Architecture(s) that use the reassociation barriers are:
   x86

   Note it is very unlikely the reassociation barriers would
   de-optimize performance on any architecture and with an imperfect
   compiler it may help performance, especially on out-of-order cpus,
   so it is suggested that the respective maintainers add them.

   Architecture maintainers are encouraged to benchmark this with
   __asm_reassociation_barrier defined to __asm__ like it is in x86.
*/


#ifndef __asm_reassociation_barrier
# define __asm_reassociation_barrier(...)
#endif

static __always_inline uint32_t
__attribute__ ((unused))
_dl_new_hash (const char *str)
{
  const unsigned char *s = (const unsigned char *) str;
  unsigned int h = 5381;
  unsigned int c0, c1;
  for (;;)
    {
      c0 = s[0];
      /* Since hashed string is normally not empty, this is unlikely on the
	 first iteration of the loop.  */
      if (__glibc_unlikely (c0 == 0))
	return h;

      c1 = s[1];
      if (c1 == 0)
	{
	  /* Ideal computational order is:
	 c0 += h;
	 h *= 32;
	 h += c0;  */
	  c0 += h;
	  __asm_reassociation_barrier("" : "+r"(h) : "r"(c0));
	  h = h * 32 + c0;
	  return h;
	}

      /* Ideal computational order is:
	 c1 += c0;
	 h *= 33 * 33;
	 c0 *= 32;
	 c1 += c0;
	 h  += c1;  */
      c1 += c0;
      __asm_reassociation_barrier("" : "+r"(c1), "+r"(c0));
      h *= 33 * 33;
      c1 += c0 * 32;
      __asm_reassociation_barrier("" : "+r"(c1));
      h += c1;
      s += 2;
    }
}

#endif /* dl-new-hash.h */
