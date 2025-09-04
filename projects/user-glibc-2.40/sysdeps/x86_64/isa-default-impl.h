/* Utility for including proper default function based on ISA level
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

#ifndef DEFAULT_IMPL_V1
# error "Must have at least ISA V1 Version"
#endif

#ifndef DEFAULT_IMPL_V2
# define DEFAULT_IMPL_V2 DEFAULT_IMPL_V1
#endif

#ifndef DEFAULT_IMPL_V3
# define DEFAULT_IMPL_V3 DEFAULT_IMPL_V2
#endif

#ifndef DEFAULT_IMPL_V4
# define DEFAULT_IMPL_V4 DEFAULT_IMPL_V3
#endif

#if MINIMUM_X86_ISA_LEVEL == 1
# define ISA_DEFAULT_IMPL DEFAULT_IMPL_V1
#elif MINIMUM_X86_ISA_LEVEL == 2
# define ISA_DEFAULT_IMPL DEFAULT_IMPL_V2
#elif MINIMUM_X86_ISA_LEVEL == 3
# define ISA_DEFAULT_IMPL DEFAULT_IMPL_V3
#elif MINIMUM_X86_ISA_LEVEL == 4
# define ISA_DEFAULT_IMPL DEFAULT_IMPL_V4
#else
# error "Unsupported ISA Level!"
#endif

#include ISA_DEFAULT_IMPL
