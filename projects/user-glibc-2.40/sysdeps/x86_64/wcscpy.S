/* ISA level static dispatch for wcscpy .S files.
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

/* wcscpy non-multiarch build is split into two files,
   wcscpy-generic.c and wcscpy.S. The wcscpy.S build is for
   ISA level >= 2 uses the optimized assembly implementations in
   multiarch/wcscpy*.S.  This must be split into two files because
   we cannot include C code from assembly or vice versa.  */

#include <isa-level.h>

#if MINIMUM_X86_ISA_LEVEL >= 2

# define WCSCPY	__wcscpy

# define DEFAULT_IMPL_V4	"multiarch/wcscpy-evex.S"
# define DEFAULT_IMPL_V3	"multiarch/wcscpy-avx2.S"
# define DEFAULT_IMPL_V2	"multiarch/wcscpy-ssse3.S"
/* isa-default-impl.h expects DEFAULT_IMPL_V1 to be defined but it
   should never be used from here.  */
# define DEFAULT_IMPL_V1	"ERROR -- Invalid ISA IMPL"

# include "isa-default-impl.h"

weak_alias (__wcscpy, wcscpy)
libc_hidden_def (__wcscpy)
#endif
