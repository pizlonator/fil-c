/* Multiple versions of wcscpy.
   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

/* Define multiple versions only for the definition in libc.  */
#if IS_IN (libc)
# define __wcscpy __redirect_wcscpy
# include <wchar.h>
# undef __wcscpy

# define SYMBOL_NAME wcscpy
# include <init-arch.h>

extern __typeof (REDIRECT_NAME) OPTIMIZE (evex) attribute_hidden;

extern __typeof (REDIRECT_NAME) OPTIMIZE (avx2) attribute_hidden;

extern __typeof (REDIRECT_NAME) OPTIMIZE (ssse3) attribute_hidden;

extern __typeof (REDIRECT_NAME) OPTIMIZE (generic) attribute_hidden;

static inline void *
IFUNC_SELECTOR (void)
{
  const struct cpu_features* cpu_features = __get_cpu_features ();

  if (X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, AVX2)
      && X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, BMI2)
      && X86_ISA_CPU_FEATURES_ARCH_P (cpu_features, AVX_Fast_Unaligned_Load, ))
    {
      if (X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, AVX512VL)
	  && X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, AVX512BW))
	return OPTIMIZE (evex);

      if (X86_ISA_CPU_FEATURES_ARCH_P (cpu_features, Prefer_No_VZEROUPPER, !))
	return OPTIMIZE (avx2);
    }

  if (X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, SSSE3))
    return OPTIMIZE (ssse3);

  return OPTIMIZE (generic);
}

libc_ifunc_redirected (__redirect_wcscpy, __wcscpy, IFUNC_SELECTOR ());
weak_alias (__wcscpy, wcscpy)
# ifdef SHARED
__hidden_ver1 (__wcscpy, __GI___wcscpy, __redirect_wcscpy)
  __attribute__((visibility ("hidden"))) __attribute_copy__ (wcscpy);
# endif
#endif
