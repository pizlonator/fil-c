/* Multiprecision generic functions.  Alpha version.
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

#ifndef __GMP_ALPHA_ARCH_H
#define __GMP_ALPHA_ARCH_H

extern mp_limb_t __udiv_qrnnd (mp_limb_t *, mp_limb_t, mp_limb_t, mp_limb_t)
     attribute_hidden;

static __always_inline void
udiv_qrnnd_alpha (mp_limb_t *q, mp_limb_t *r, mp_limb_t n1, mp_limb_t n0,
		  mp_limb_t d)
{
  *q = __udiv_qrnnd (r, n1, n0, d);
}
#undef UDIV_NEEDS_NORMALIZATION
#define UDIV_NEEDS_NORMALIZATION 0
# undef udiv_qrnnd
# define udiv_qrnnd(__q, __r, __n1, __n0, __d) \
  udiv_qrnnd_alpha (&__q, &__r, __n1, __n0, __d)

#include <sysdeps/generic/gmp-arch.h>

#endif
