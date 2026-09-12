/* Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

/* MIPSr6 has MADDF.s/MADDF.d instructions, which are fused.  In MIPS
   ISA, double support can be subsetted.  Only FMAF is enabled for this
   case.  */

#include <isarev.h>

#if __mips_isa_rev >= 6
# ifdef __mips_single_float
#  define USE_FMA_BUILTIN 0
# else
#  define USE_FMA_BUILTIN 1
# endif
# define USE_FMAF_BUILTIN 1
#else
# define USE_FMA_BUILTIN 0
# define USE_FMAF_BUILTIN 0
#endif
#define USE_FMAL_BUILTIN 0
#define USE_FMAF128_BUILTIN 0
