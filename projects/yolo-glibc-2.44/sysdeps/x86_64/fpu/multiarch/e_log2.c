/* Multiple versions of log2.
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
# include <libm-alias-finite.h>

extern double __redirect_log2 (double);

# define SYMBOL_NAME log2
# include "ifunc-fma.h"

libc_ifunc_redirected (__redirect_log2, __log2, IFUNC_SELECTOR ());

# ifdef SHARED
__hidden_ver1 (__log2, __GI___log2, __redirect_log2)
  __attribute__ ((visibility ("hidden")));

versioned_symbol (libm, __ieee754_log2, log2, GLIBC_2_29);
libm_alias_double_other (__log2, log2)
# else
libm_alias_double (__log2, log2)
# endif

strong_alias (__log2, __ieee754_log2)
libm_alias_finite (__log2, __log2)

# define __log2 __log2_sse2
#endif
#include <sysdeps/ieee754/dbl-64/e_log2.c>
