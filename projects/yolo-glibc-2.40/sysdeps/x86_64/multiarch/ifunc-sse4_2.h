/* Common definition for ifunc selections optimized with SSE2 and SSE4.2.
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

#include <init-arch.h>

extern __typeof (REDIRECT_NAME) OPTIMIZE (generic) attribute_hidden;
extern __typeof (REDIRECT_NAME) OPTIMIZE (sse42) attribute_hidden;

static inline void *
IFUNC_SELECTOR (void)
{
  const struct cpu_features* cpu_features = __get_cpu_features ();

  /* This function uses the `pcmpstri` sse4.2 instruction which can be
     slow on some CPUs.  This normally would be guarded by a
     Slow_SSE4_2 check, but since there is no other optimized
     implementation its best to keep it regardless.  If an optimized
     fallback is added add a X86_ISA_CPU_FEATURE_ARCH_P (cpu_features,
     Slow_SSE4_2) check.  */
  if (X86_ISA_CPU_FEATURE_USABLE_P (cpu_features, SSE4_2))
    return OPTIMIZE (sse42);

  return OPTIMIZE (generic);
}
