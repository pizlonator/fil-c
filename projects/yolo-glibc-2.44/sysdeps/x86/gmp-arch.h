/* Multiprecision generic functions.  x86 version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#ifndef __GMP_X64_ARCH_H
#define __GMP_X64_ARCH_H

#include <gmp.h>

static __always_inline void
udiv_qrnnd_x86 (mp_limb_t *q, mp_limb_t *r, mp_limb_t n1, mp_limb_t n0,
		mp_limb_t d)
{
#ifdef __x86_64__
  asm ("div{q} %4"
       : "=a" (*q),
         "=d" (*r)
       : "0" (n0),
	 "1" (n1),
	 "rm" (d));
#else
  asm ("div{l} %4"
       : "=a" (*q),
         "=d" (*r)
       : "0" (n0),
	 "1" (n1),
	 "rm" (d));
#endif
}
#define UDIV_NEEDS_NORMALIZATION 0
#define udiv_qrnnd(__q, __r, __n1, __n0, __d) \
  udiv_qrnnd_x86 (&__q, &__r, __n1, __n0, __d)

#include <sysdeps/generic/gmp-arch.h>

#endif
