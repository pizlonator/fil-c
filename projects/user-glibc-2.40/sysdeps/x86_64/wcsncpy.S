/* ISA level static dispatch for wcsncpy .S files.
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

/* wcsncpy non-multiarch build is split into two files,
   wcsncpy-generic.c and wcsncpy.S. The wcsncpy-generic.c build is
   for ISA level <= 1 and just uses multiarch/wcsncpy-generic.c.
   This must be split into two files because we cannot include C
   code from assembly or vice versa.  */

#include <isa-level.h>

#if MINIMUM_X86_ISA_LEVEL >= 3

# define WCSNCPY	__wcsncpy

# define DEFAULT_IMPL_V4	"multiarch/wcsncpy-evex.S"
# define DEFAULT_IMPL_V3	"multiarch/wcsncpy-avx2.S"
/* isa-default-impl.h expects DEFAULT_IMPL_V1 to be defined but it
   should never be used from here.  */
# define DEFAULT_IMPL_V1	"ERROR -- Invalid ISA IMPL"

# include "isa-default-impl.h"

weak_alias (__wcsncpy, wcsncpy)
libc_hidden_def (__wcsncpy)
#endif
