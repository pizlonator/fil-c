/* Multiple versions of expm1.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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

#include <sysdeps/x86/isa-level.h>
#if MINIMUM_X86_ISA_LEVEL < AVX2_X86_ISA_LEVEL
# include <libm-alias-double.h>

extern double __redirect_expm1 (double);

# define SYMBOL_NAME expm1
# include "ifunc-fma.h"

libc_ifunc_redirected (__redirect_expm1, __expm1, IFUNC_SELECTOR ());
libm_alias_double (__expm1, expm1)

# define __expm1 __expm1_sse2

/* NB: __expm1 may be expanded to __expm1_sse2 in the following
   prototypes.  */
extern long double __expm1l (long double);
extern long double __expm1f128 (long double);

#endif
#include <sysdeps/ieee754/dbl-64/s_expm1.c>
