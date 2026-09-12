/* Helper functions for manipulating memory protection keys.
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

#ifndef _ARCH_PKEY_H
#define _ARCH_PKEY_H

#include <sys/cdefs.h>

#define S1POE_PERM_NO_ACCESS	0b0000UL
#define S1POE_PERM_R		0b0001UL
#define S1POE_PERM_X		0b0010UL
#define S1POE_PERM_RX		0b0011UL
#define S1POE_PERM_W		0b0100UL
#define S1POE_PERM_RW		0b0101UL
#define S1POE_PERM_WX		0b0110UL
#define S1POE_PERM_RWX		0b0111UL

#define S1POE_PERM_MASK 	0b1111UL

#define S1POE_BITS_PER_POI      4UL

/* Return the value of the POR_EL0 register.  */
static __always_inline unsigned long
pkey_read (void)
{
  unsigned long r;
  __asm__ volatile ("mrs %0, s3_3_c10_c2_4" : "=r" (r));
  return r;
}

/* Overwrite the POR_EL0 register with VALUE.  */
static __always_inline void
pkey_write (unsigned long value)
{
  __asm__ volatile ("msr s3_3_c10_c2_4, %0; isb" : : "r" (value));
}

#endif /* _ARCH_PKEY_H */
