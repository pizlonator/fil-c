/* Symbol rediretion for loader/static initialization code.
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

#ifndef _DL_IFUNC_GENERIC_H
#define _DL_IFUNC_GENERIC_H

#ifndef SHARED

#include <isa-level.h>

#if MINIMUM_X86_ISA_LEVEL >= 4
# define HAVE_MEMSET_IFUNC_GENERIC "__memset_evex_unaligned"
#elif MINIMUM_X86_ISA_LEVEL == 3
# define HAVE_MEMSET_IFUNC_GENERIC "__memset_avx2_unaligned"
#else
# define HAVE_MEMSET_IFUNC_GENERIC "__memset_sse2_unaligned"
#endif

asm ("memset = " HAVE_MEMSET_IFUNC_GENERIC);


#if MINIMUM_X86_ISA_LEVEL >= 4
# define HAVE_MEMCMP_IFUNC_GENERIC "__memcmp_evex_movbe"
#elif MINIMUM_X86_ISA_LEVEL == 3
# define HAVE_MEMCMP_IFUNC_GENERIC "__memcmp_avx2_movbe"
#else
# define HAVE_MEMCMP_IFUNC_GENERIC "__memcmp_sse2"
#endif

asm ("memcmp = " HAVE_MEMCMP_IFUNC_GENERIC);

#endif /* SHARED */

#endif
