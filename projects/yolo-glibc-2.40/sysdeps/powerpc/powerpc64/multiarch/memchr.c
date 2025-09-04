/* Multiple versions of memchr.
   Copyright (C) 2013-2024 Free Software Foundation, Inc.
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

#if IS_IN (libc)
# include <string.h>
# include <shlib-compat.h>
# include "init-arch.h"

extern __typeof (__memchr) __memchr_ppc attribute_hidden;
extern __typeof (__memchr) __memchr_power7 attribute_hidden;
extern __typeof (__memchr) __memchr_power8 attribute_hidden;

# ifdef __LITTLE_ENDIAN__
extern __typeof (__memchr) __memchr_power10 attribute_hidden;
# endif
/* Avoid DWARF definition DIE on ifunc symbol so that GDB can handle
   ifunc symbol properly.  */
libc_ifunc (__memchr,
# ifdef __LITTLE_ENDIAN__
	    (hwcap2 & PPC_FEATURE2_ARCH_3_1
	     && hwcap & PPC_FEATURE_HAS_VSX)
	    ? __memchr_power10 :
# endif
	      (hwcap2 & PPC_FEATURE2_ARCH_2_07
	      && hwcap & PPC_FEATURE_HAS_ALTIVEC)
	      ? __memchr_power8 :
	        (hwcap & PPC_FEATURE_ARCH_2_06)
	        ? __memchr_power7
	        : __memchr_ppc);

weak_alias (__memchr, memchr)
libc_hidden_builtin_def (memchr)
#else
#include <string/memchr.c>
#endif
