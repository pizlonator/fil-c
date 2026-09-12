/* Multiple versions of memcmpeq. PowerPC64 version.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

/* Define multiple versions only for definition in libc.  */
#if IS_IN (libc)
# define __memcmpeq __redirect___memcmpeq
# include <string.h>
# include <shlib-compat.h>
# include "init-arch.h"

/* Reuse the existing optimized memcmp variants for pre-POWER10 hardware
   as memcmp is a superset */
extern __typeof (memcmp) __memcmp_ppc attribute_hidden;
extern __typeof (memcmp) __memcmp_power4 attribute_hidden;
extern __typeof (memcmp) __memcmp_power7 attribute_hidden;
extern __typeof (memcmp) __memcmp_power8 attribute_hidden;
extern __typeof (__memcmpeq) __memcmpeq_power10 attribute_hidden;
# undef __memcmpeq

/* Avoid DWARF definition DIE on ifunc symbol so that GDB can handle
   ifunc symbol properly.  */
libc_ifunc_redirected (__redirect___memcmpeq, __memcmpeq,
#ifdef __LITTLE_ENDIAN__
				(hwcap2 & PPC_FEATURE2_ARCH_3_1
				 && hwcap & PPC_FEATURE_HAS_VSX)
				 ? __memcmpeq_power10 :
#endif
		       (hwcap2 & PPC_FEATURE2_ARCH_2_07
			&& hwcap & PPC_FEATURE_HAS_ALTIVEC)
		       ? __memcmp_power8 :
		       (hwcap & PPC_FEATURE_ARCH_2_06)
		       ? __memcmp_power7
		       : (hwcap & PPC_FEATURE_POWER4)
			 ? __memcmp_power4
			 : __memcmp_ppc);
# ifdef SHARED
__hidden_ver1 (__memcmpeq, __GI___memcmpeq, __redirect___memcmpeq)
    __attribute__ ((visibility ("hidden"))) __attribute_copy__ (__memcmpeq);
# endif
#else
#include <string/memcmp.c>
#endif
