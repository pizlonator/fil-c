/* Multiple versions of sinf.
   Copyright (C) 2017-2026 Free Software Foundation, Inc.
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
# include <libm-alias-float.h>

extern float __redirect_sinf (float);

# define SYMBOL_NAME sinf
# include "ifunc-fma.h"

libc_ifunc_redirected (__redirect_sinf, __sinf, IFUNC_SELECTOR ());

libm_alias_float (__sin, sin)
#else
# include <sysdeps/ieee754/flt-32/s_sinf.c>
#endif
