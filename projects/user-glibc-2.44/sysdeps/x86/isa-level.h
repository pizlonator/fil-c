/* Header defining the minimum x86 ISA level
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   In addition to the permissions in the GNU Lesser General Public
   License, the Free Software Foundation gives you unlimited
   permission to link the compiled version of this file with other
   programs, and to distribute those programs without any restriction
   coming from the use of this file.  (The Lesser General Public
   License restrictions do apply in other respects; for example, they
   cover modification of the file, and distribution when not linked
   into another program.)

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _ISA_LEVEL_H
#define _ISA_LEVEL_H

#if defined __SSE__ && defined __SSE2__
/* NB: ISAs, excluding MMX, in x86-64 ISA level baseline are used.  */
# define __X86_ISA_V1 1
#else
# define __X86_ISA_V1 0
#endif

#ifdef __x86_64__
# ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16
#  define __GCC_HAVE_SYNC_COMPARE_AND_SWAP
# endif
#else
# ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
#  define __GCC_HAVE_SYNC_COMPARE_AND_SWAP
# endif
#endif

#if __X86_ISA_V1 && defined __GCC_HAVE_SYNC_COMPARE_AND_SWAP		      \
    && defined HAVE_X86_LAHF_SAHF && defined __POPCNT__ && defined __SSE3__   \
    && defined __SSSE3__ && defined __SSE4_1__ && defined __SSE4_2__
/* NB: ISAs in x86-64 ISA level v2 are used.  */
# define __X86_ISA_V2 1
#else
# define __X86_ISA_V2 0
#endif

#if __X86_ISA_V2 && defined __AVX__ && defined __AVX2__ && defined __F16C__   \
    && defined __FMA__ && defined __LZCNT__ && defined HAVE_X86_MOVBE         \
    && defined __BMI__ && defined __BMI2__
/* NB: ISAs in x86-64 ISA level v3 are used.  */
# define __X86_ISA_V3 1
#else
# define __X86_ISA_V3 0
#endif

#if __X86_ISA_V3 && defined __AVX512F__ && defined __AVX512BW__               \
    && defined __AVX512CD__ && defined __AVX512DQ__ && defined __AVX512VL__
/* NB: ISAs in x86-64 ISA level v4 are used.  */
# define __X86_ISA_V4 1
#else
# define __X86_ISA_V4 0
#endif

#ifndef MINIMUM_X86_ISA_LEVEL
# define MINIMUM_X86_ISA_LEVEL                                                 \
  (__X86_ISA_V1 + __X86_ISA_V2 + __X86_ISA_V3 + __X86_ISA_V4)
#endif

/* Depending on the minimum ISA level, a feature check result can be a
   compile-time constant.. */


/* For CPU_FEATURE_USABLE_P.  */

/* ISA level >= 4 guaranteed includes.  */
#define AVX512F_X86_ISA_LEVEL 4
#define AVX512VL_X86_ISA_LEVEL 4
#define AVX512BW_X86_ISA_LEVEL 4
#define AVX512DQ_X86_ISA_LEVEL 4

/* ISA level >= 3 guaranteed includes.  */
#define AVX_X86_ISA_LEVEL 3
#define AVX2_X86_ISA_LEVEL 3
#define BMI1_X86_ISA_LEVEL 3
#define BMI2_X86_ISA_LEVEL 3
#define LZCNT_X86_ISA_LEVEL 3
#define MOVBE_X86_ISA_LEVEL 3

/* ISA level >= 2 guaranteed includes.  */
#define SSE4_2_X86_ISA_LEVEL 2
#define SSE4_1_X86_ISA_LEVEL 2
#define SSSE3_X86_ISA_LEVEL 2


/* For X86_ISA_CPU_FEATURES_ARCH_P.  */

/* NB: This feature is enabled when ISA level >= 3, which was disabled
   for the following CPUs:
        - AMD Excavator
   when ISA level < 3.  */
#define AVX_Fast_Unaligned_Load_X86_ISA_LEVEL 3

/* NB: This feature is disabled when ISA level >= 3, which was enabled
   for the following CPUs:
        - Intel KNL
   when ISA level < 3.  */
#define Prefer_No_VZEROUPPER_X86_ISA_LEVEL 3

/* NB: This feature is disable when ISA level >= 3.  All CPUs with
   this feature don't run on glibc built with ISA level >= 3.  */
#define Slow_SSE42_X86_ISA_LEVEL 3

/* Feature(s) enabled when ISA level >= 2.  */
#define Fast_Unaligned_Load_X86_ISA_LEVEL 2

/* NB: This feature is disable when ISA level >= 2, which was enabled
   for the early Atom CPUs.  */
#define Slow_BSF_X86_ISA_LEVEL 2


/* Both X86_ISA_CPU_FEATURE_USABLE_P and X86_ISA_CPU_FEATURES_ARCH_P
   macros are wrappers for the respective CPU_FEATURE{S}_{USABLE|ARCH}_P
   runtime checks.  They differ in two ways.

    1.  The USABLE_P version is evaluated to true when the feature
        is enabled.

    2.  The ARCH_P version has a third argument `not`.  The `not`
        argument can either be `!` or empty.  If the feature is
        enabled above an ISA level, the third argument should be empty
        and the expression is evaluated to true when the feature is
        enabled.  If the feature is disabled above an ISA level, the
        third argument should be `!` and the expression is evaluated
        to true when the feature is disabled.
 */

#define X86_ISA_CPU_FEATURE_USABLE_P(ptr, name)                        \
  (((name##_X86_ISA_LEVEL) <= MINIMUM_X86_ISA_LEVEL)                   \
   || CPU_FEATURE_USABLE_P (ptr, name))

#define X86_ISA_CPU_FEATURES_ARCH_P(ptr, name, not)                    \
  (((name##_X86_ISA_LEVEL) <= MINIMUM_X86_ISA_LEVEL)                   \
   || not CPU_FEATURES_ARCH_P (ptr, name))

#define ISA_SHOULD_BUILD(isa_build_level)                              \
  (MINIMUM_X86_ISA_LEVEL <= (isa_build_level) && IS_IN (libc))         \
   || defined ISA_DEFAULT_IMPL

#endif
