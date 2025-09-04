/* Common definition for __memcmpeq ifunc selections.
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

# include <init-arch.h>

extern __typeof (REDIRECT_NAME) OPTIMIZE1 (evex) attribute_hidden;

extern __typeof (REDIRECT_NAME) OPTIMIZE1 (avx2) attribute_hidden;
extern __typeof (REDIRECT_NAME) OPTIMIZE1 (avx2_rtm) attribute_hidden;

extern __typeof (REDIRECT_NAME) OPTIMIZE1 (sse2) attribute_hidden;

static inline void *
IFUNC_SELECTOR (void)
{
  const struct cpu_features *cpu_features = __get_cpu_features ();

  if (X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, AVX2)
      && X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, BMI2)
      && X86_ISA_CPU_FEATURES_ARCH_P (cpu_features,
				      AVX_Fast_Unaligned_Load, ))
    {
      if (X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, AVX512VL)
	  && X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, AVX512BW))
	return OPTIMIZE1 (evex);

      if (CPU_FEATURE_USABLE_P (cpu_features, RTM))
	return OPTIMIZE1 (avx2_rtm);

      if (X86_ISA_CPU_FEATURES_ARCH_P (cpu_features,
				       Prefer_No_VZEROUPPER, !))
	return OPTIMIZE1 (avx2);
    }

  return OPTIMIZE1 (sse2);
}
