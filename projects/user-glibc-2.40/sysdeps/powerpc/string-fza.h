/* Zero byte detection; basics.  PowerPC version.
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
   <http://www.gnu.org/licenses/>.  */

#ifndef _POWERPC_STRING_FZA_H
#define _POWERPC_STRING_FZA_H 1

/* PowerISA 2.05 (POWER6) provides cmpb instruction.  */
#ifdef _ARCH_PWR6
# include <string-misc.h>
# include <string-optype.h>

/* The functions return a byte mask.  */
typedef op_t find_t;

/* This function returns 0xff for each byte that is
   equal between X1 and X2.  */

static __always_inline find_t
find_eq_all (op_t x1, op_t x2)
{
  return __builtin_cmpb (x1, x2);
}

/* This function returns 0xff for each byte that is zero in X.  */

static __always_inline find_t
find_zero_all (op_t x)
{
  return find_eq_all (x, 0);
}

/* Identify zero bytes in X1 or equality between X1 and X2.  */

static __always_inline find_t
find_zero_eq_all (op_t x1, op_t x2)
{
  return find_zero_all (x1) | find_eq_all (x1, x2);
}

/* Identify zero bytes in X1 or inequality between X1 and X2.  */

static __always_inline find_t
find_zero_ne_all (op_t x1, op_t x2)
{
  return find_zero_all (x1) | ~find_eq_all (x1, x2);
}

/* Define the "inexact" versions in terms of the exact versions.  */
# define find_zero_low		find_zero_all
# define find_eq_low		find_eq_all
# define find_zero_eq_low	find_zero_eq_all
#else
# include <sysdeps/generic/string-fza.h>
#endif /* _ARCH_PWR6  */

#endif /* _POWERPC_STRING_FZA_H  */
