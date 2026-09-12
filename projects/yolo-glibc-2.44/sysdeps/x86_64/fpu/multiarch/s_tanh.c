/* Multiple versions of tanh.
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

#include <sysdeps/x86/isa-level.h>
#if MINIMUM_X86_ISA_LEVEL < AVX2_X86_ISA_LEVEL
#include <libm-alias-double.h>

extern double __redirect_tanh (double);

# define SYMBOL_NAME tanh
# include "ifunc-fma.h"

libc_ifunc_redirected (__redirect_tanh, tanh, IFUNC_SELECTOR ());

# define __tanh __tanh_sse2
# undef libm_alias_double
# define libm_alias_double(a, b)
#endif
#include <sysdeps/ieee754/dbl-64/s_tanh.c>
