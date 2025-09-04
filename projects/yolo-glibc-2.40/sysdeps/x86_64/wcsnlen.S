/* wcsnlen dispatch for RTLD and non-multiarch .c files
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

/* wcsnlen non-multiarch build is split into two files,
   wcsnlen-generic.c and wcsnlen.S. The wcsnlen.S build is for
   ISA level >= 2 uses the optimized assembly implementations in
   multiarch/wcsnlen*.S.  This must be split into two files because
   we cannot include C code from assembly or vice versa.  */

#include <isa-level.h>

#if MINIMUM_X86_ISA_LEVEL >= 2

# define WCSNLEN	__wcsnlen
/* This symbol must stay linked to the name in wcslen.S.  */
#if IS_IN (rtld)
# define OVERFLOW_STRLEN	__wcslen
#else
# define OVERFLOW_STRLEN	HIDDEN_JUMPTARGET (__wcslen)
#endif

# define DEFAULT_IMPL_V2	"multiarch/wcsnlen-sse4_1.S"
# define DEFAULT_IMPL_V3	"multiarch/wcsnlen-avx2.S"
# define DEFAULT_IMPL_V4	"multiarch/wcsnlen-evex.S"

/* isa-default-impl.h expects DEFAULT_IMPL_V1 to be defined but it
   should never be used from here.  */
# define DEFAULT_IMPL_V1	"ERROR -- Invalid ISA IMPL"

# include "isa-default-impl.h"

weak_alias (__wcsnlen, wcsnlen)
libc_hidden_def (__wcsnlen)
#endif
