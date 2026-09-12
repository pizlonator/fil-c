/* Multiple versions of __ceilf.
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
#if MINIMUM_X86_ISA_LEVEL < SSE4_1_X86_ISA_LEVEL
# define NO_MATH_REDIRECT
# include <libm-alias-float.h>

# define ceilf __redirect_ceilf
# define __ceilf __redirect___ceilf
# include <math.h>
# undef ceilf
# undef __ceilf

# define SYMBOL_NAME ceilf
# include "ifunc-sse4_1.h"

libc_ifunc_redirected (__redirect_ceilf, __ceilf, IFUNC_SELECTOR ());
libm_alias_float (__ceil, ceil)
#endif
