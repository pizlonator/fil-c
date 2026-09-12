/* Multiple versions of modf
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
#if MINIMUM_X86_ISA_LEVEL < AVX_X86_ISA_LEVEL
# define NO_MATH_REDIRECT
# include <libm-alias-double.h>

# define modf __redirect_modf
# define __modf __redirect___modf
# include <math.h>
# undef modf
# undef __modf

# define SYMBOL_NAME modf
# include "ifunc-sse4_1-avx.h"

libc_ifunc_redirected (__redirect_modf, __modf, IFUNC_SELECTOR ());
libm_alias_double (__modf, modf)
# if MINIMUM_X86_ISA_LEVEL == SSE4_1_X86_ISA_LEVEL
#  define __modf __modf_sse41
# else
#  define __modf __modf_sse2
# endif
#endif
#include <math_private.h>
#define trunc __trunc
#include <sysdeps/ieee754/dbl-64/s_modf.c>
