/* Hook for build varshift for non-multiarch build.  Needed for the
   ISA level >= 2 because strspn-sse4, strcspn-sse4, and strpbrk-sse4
   all have a dependency on varshift.c.
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


#include <isa-level.h>

#if MINIMUM_X86_ISA_LEVEL >= 2
# include "multiarch/varshift.c"
#endif
