/* cipher-gcm-intel-pclmul.c  -  Intel PCLMUL accelerated Galois Counter Mode
 *                               implementation
 * Copyright (C) 2013-2014,2019,2022 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "g10lib.h"
#include "cipher.h"
#include "bufhelp.h"
#include "./cipher-internal.h"


#ifdef GCM_USE_INTEL_PCLMUL


#if _GCRY_GCC_VERSION >= 40400 /* 4.4 */
/* Prevent compiler from issuing SSE instructions between asm blocks. */
#  pragma GCC target("no-sse")
#endif
#if __clang__
#  pragma clang attribute push (__attribute__((target("no-sse"))), apply_to = function)
#endif


#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NO_INSTRUMENT_FUNCTION __attribute__((no_instrument_function))

#define ASM_FUNC_ATTR        NO_INSTRUMENT_FUNCTION
#define ASM_FUNC_ATTR_INLINE ASM_FUNC_ATTR ALWAYS_INLINE


#define GCM_INTEL_USE_VPCLMUL_AVX2         (1 << 0)
#define GCM_INTEL_AGGR8_TABLE_INITIALIZED  (1 << 1)
#define GCM_INTEL_AGGR16_TABLE_INITIALIZED (1 << 2)
#define GCM_INTEL_USE_VPCLMUL_AVX512       (1 << 3)
#define GCM_INTEL_AGGR32_TABLE_INITIALIZED (1 << 4)


/*
 Intel PCLMUL ghash based on white paper:
  "Intel® Carry-Less Multiplication Instruction and its Usage for Computing the
   GCM Mode - Rev 2.01"; Shay Gueron, Michael E. Kounavis.
 */
static ASM_FUNC_ATTR_INLINE
void reduction(void)
{
  /* input: <xmm1:xmm3> */

  asm volatile (/* first phase of the reduction */
                "movdqa %%xmm3, %%xmm6\n\t"
                "movdqa %%xmm3, %%xmm5\n\t"
                "psllq $1, %%xmm6\n\t"  /* packed right shifting << 63 */
                "pxor %%xmm3, %%xmm6\n\t"
                "psllq $57, %%xmm5\n\t"  /* packed right shifting << 57 */
                "psllq $62, %%xmm6\n\t"  /* packed right shifting << 62 */
                "pxor %%xmm5, %%xmm6\n\t" /* xor the shifted versions */
                "pshufd $0x6a, %%xmm6, %%xmm5\n\t"
                "pshufd $0xae, %%xmm6, %%xmm6\n\t"
                "pxor %%xmm5, %%xmm3\n\t" /* first phase of the reduction
                                             complete */

                /* second phase of the reduction */
                "pxor %%xmm3, %%xmm1\n\t" /* xor the shifted versions */
                "psrlq $1, %%xmm3\n\t"    /* packed left shifting >> 1 */
                "pxor %%xmm3, %%xmm6\n\t"
                "psrlq $1, %%xmm3\n\t"    /* packed left shifting >> 2 */
                "pxor %%xmm3, %%xmm1\n\t"
                "psrlq $5, %%xmm3\n\t"    /* packed left shifting >> 7 */
                "pxor %%xmm3, %%xmm6\n\t"
                "pxor %%xmm6, %%xmm1\n\t" /* the result is in xmm1 */
                ::: "memory" );
}

static ASM_FUNC_ATTR_INLINE
void gfmul_pclmul(void)
{
  /* Input: XMM0 and XMM1, Output: XMM1. Input XMM0 stays unmodified.
     Input must be converted to little-endian.
   */
  asm volatile (/* gfmul, xmm0 has operator a and xmm1 has operator b. */
                "pshufd $78, %%xmm0, %%xmm2\n\t"
                "pshufd $78, %%xmm1, %%xmm4\n\t"
                "pxor %%xmm0, %%xmm2\n\t" /* xmm2 holds a0+a1 */
                "pxor %%xmm1, %%xmm4\n\t" /* xmm4 holds b0+b1 */

                "movdqa %%xmm0, %%xmm3\n\t"
                "pclmulqdq $0, %%xmm1, %%xmm3\n\t"  /* xmm3 holds a0*b0 */
                "pclmulqdq $17, %%xmm0, %%xmm1\n\t" /* xmm6 holds a1*b1 */
                "movdqa %%xmm3, %%xmm5\n\t"
                "pclmulqdq $0, %%xmm2, %%xmm4\n\t"  /* xmm4 holds (a0+a1)*(b0+b1) */

                "pxor %%xmm1, %%xmm5\n\t" /* xmm5 holds a0*b0+a1*b1 */
                "pxor %%xmm5, %%xmm4\n\t" /* xmm4 holds a0*b0+a1*b1+(a0+a1)*(b0+b1) */
                "movdqa %%xmm4, %%xmm5\n\t"
                "psrldq $8, %%xmm4\n\t"
                "pslldq $8, %%xmm5\n\t"
                "pxor %%xmm5, %%xmm3\n\t"
                "pxor %%xmm4, %%xmm1\n\t" /* <xmm1:xmm3> holds the result of the
                                             carry-less multiplication of xmm0
                                             by xmm1 */
                ::: "memory" );

  reduction();
}

#define GFMUL_AGGR4_ASM_1(be_to_le)                                            \
    /* perform clmul and merge results... */                                   \
    "movdqu 2*16(%[h_table]), %%xmm2\n\t" /* Load H4 */                        \
    "movdqu 0*16(%[buf]), %%xmm5\n\t"                                          \
    be_to_le("pshufb %[be_mask], %%xmm5\n\t") /* be => le */                   \
    "pxor %%xmm5, %%xmm1\n\t"                                                  \
                                                                               \
    "pshufd $78, %%xmm2, %%xmm5\n\t"                                           \
    "pshufd $78, %%xmm1, %%xmm4\n\t"                                           \
    "pxor %%xmm2, %%xmm5\n\t" /* xmm5 holds 4:a0+a1 */                         \
    "pxor %%xmm1, %%xmm4\n\t" /* xmm4 holds 4:b0+b1 */                         \
    "movdqa %%xmm2, %%xmm3\n\t"                                                \
    "pclmulqdq $0, %%xmm1, %%xmm3\n\t"   /* xmm3 holds 4:a0*b0 */              \
    "pclmulqdq $17, %%xmm2, %%xmm1\n\t"  /* xmm1 holds 4:a1*b1 */              \
    "pclmulqdq $0, %%xmm5, %%xmm4\n\t"   /* xmm4 holds 4:(a0+a1)*(b0+b1) */    \
                                                                               \
    "movdqu 1*16(%[h_table]), %%xmm5\n\t" /* Load H3 */                        \
    "movdqu 1*16(%[buf]), %%xmm2\n\t"                                          \
    be_to_le("pshufb %[be_mask], %%xmm2\n\t") /* be => le */                   \
                                                                               \
    "pshufd $78, %%xmm5, %%xmm0\n\t"                                           \
    "pshufd $78, %%xmm2, %%xmm7\n\t"                                           \
    "pxor %%xmm5, %%xmm0\n\t" /* xmm0 holds 3:a0+a1 */                         \
    "pxor %%xmm2, %%xmm7\n\t" /* xmm7 holds 3:b0+b1 */                         \
    "movdqa %%xmm5, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm2, %%xmm6\n\t"  /* xmm6 holds 3:a0*b0 */               \
    "pclmulqdq $17, %%xmm5, %%xmm2\n\t" /* xmm2 holds 3:a1*b1 */               \
    "pclmulqdq $0, %%xmm0, %%xmm7\n\t" /* xmm7 holds 3:(a0+a1)*(b0+b1) */      \
                                                                               \
    "movdqu 2*16(%[buf]), %%xmm5\n\t"                                          \
    be_to_le("pshufb %[be_mask], %%xmm5\n\t") /* be => le */                   \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 3+4:a0*b0 */                       \
    "pxor %%xmm2, %%xmm1\n\t" /* xmm1 holds 3+4:a1*b1 */                       \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 3+4:(a0+a1)*(b0+b1) */             \
                                                                               \
    "movdqu 0*16(%[h_table]), %%xmm2\n\t" /* Load H2 */                        \
                                                                               \
    "pshufd $78, %%xmm2, %%xmm0\n\t"                                           \
    "pshufd $78, %%xmm5, %%xmm7\n\t"                                           \
    "pxor %%xmm2, %%xmm0\n\t" /* xmm0 holds 2:a0+a1 */                         \
    "pxor %%xmm5, %%xmm7\n\t" /* xmm7 holds 2:b0+b1 */                         \
    "movdqa %%xmm2, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm5, %%xmm6\n\t"  /* xmm6 holds 2:a0*b0 */               \
    "pclmulqdq $17, %%xmm2, %%xmm5\n\t" /* xmm5 holds 2:a1*b1 */               \
    "pclmulqdq $0, %%xmm0, %%xmm7\n\t" /* xmm7 holds 2:(a0+a1)*(b0+b1) */      \
                                                                               \
    "movdqu 3*16(%[buf]), %%xmm2\n\t"                                          \
    be_to_le("pshufb %[be_mask], %%xmm2\n\t") /* be => le */                   \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 2+3+4:a0*b0 */                     \
    "pxor %%xmm5, %%xmm1\n\t" /* xmm1 holds 2+3+4:a1*b1 */                     \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 2+3+4:(a0+a1)*(b0+b1) */

#define GFMUL_AGGR4_ASM_2()                                                    \
    "movdqu %[h_1], %%xmm5\n\t" /* Load H1 */                                  \
                                                                               \
    "pshufd $78, %%xmm5, %%xmm0\n\t"                                           \
    "pshufd $78, %%xmm2, %%xmm7\n\t"                                           \
    "pxor %%xmm5, %%xmm0\n\t" /* xmm0 holds 1:a0+a1 */                         \
    "pxor %%xmm2, %%xmm7\n\t" /* xmm7 holds 1:b0+b1 */                         \
    "movdqa %%xmm5, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm2, %%xmm6\n\t"  /* xmm6 holds 1:a0*b0 */               \
    "pclmulqdq $17, %%xmm5, %%xmm2\n\t" /* xmm2 holds 1:a1*b1 */               \
    "pclmulqdq $0, %%xmm0, %%xmm7\n\t" /* xmm7 holds 1:(a0+a1)*(b0+b1) */      \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 1+2+3+4:a0*b0 */                   \
    "pxor %%xmm2, %%xmm1\n\t" /* xmm1 holds 1+2+3+4:a1*b1 */                   \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 1+2+3+4:(a0+a1)*(b0+b1) */         \
                                                                               \
    /* aggregated reduction... */                                              \
    "movdqa %%xmm3, %%xmm5\n\t"                                                \
    "pxor %%xmm1, %%xmm5\n\t" /* xmm5 holds a0*b0+a1*b1 */                     \
    "pxor %%xmm5, %%xmm4\n\t" /* xmm4 holds a0*b0+a1*b1+(a0+a1)*(b0+b1) */     \
    "movdqa %%xmm4, %%xmm5\n\t"                                                \
    "psrldq $8, %%xmm4\n\t"                                                    \
    "pslldq $8, %%xmm5\n\t"                                                    \
    "pxor %%xmm5, %%xmm3\n\t"                                                  \
    "pxor %%xmm4, %%xmm1\n\t" /* <xmm1:xmm3> holds the result of the           \
                                  carry-less multiplication of xmm0            \
                                  by xmm1 */

#define be_to_le(...) __VA_ARGS__
#define le_to_le(...) /*_*/

static ASM_FUNC_ATTR_INLINE void
gfmul_pclmul_aggr4(const void *buf, const void *h_1, const void *h_table,
		   const unsigned char *be_mask)
{
  /* Input:
      Hash: XMM1
     Output:
      Hash: XMM1
   */
  asm volatile (GFMUL_AGGR4_ASM_1(be_to_le)
                :
                : [buf] "r" (buf),
                  [h_table] "r" (h_table),
                  [be_mask] "m" (*be_mask)
                : "memory" );

  asm volatile (GFMUL_AGGR4_ASM_2()
                :
                : [h_1] "m" (*(const unsigned char *)h_1)
                : "memory" );

  reduction();
}

static ASM_FUNC_ATTR_INLINE void
gfmul_pclmul_aggr4_le(const void *buf, const void *h_1, const void *h_table)
{
  /* Input:
      Hash: XMM1
     Output:
      Hash: XMM1
   */
  asm volatile (GFMUL_AGGR4_ASM_1(le_to_le)
                :
                : [buf] "r" (buf),
                  [h_table] "r" (h_table)
                : "memory" );

  asm volatile (GFMUL_AGGR4_ASM_2()
                :
                : [h_1] "m" (*(const unsigned char *)h_1)
                : "memory" );

  reduction();
}

#ifdef __x86_64__

#define GFMUL_AGGR8_ASM(be_to_le)                                              \
    /* Load H6, H7, H8. */                                                     \
    "movdqu 6*16(%[h_table]), %%xmm10\n\t"                                     \
    "movdqu 5*16(%[h_table]), %%xmm9\n\t"                                      \
    "movdqu 4*16(%[h_table]), %%xmm8\n\t"                                      \
                                                                               \
    /* perform clmul and merge results... */                                   \
    "movdqu 0*16(%[buf]), %%xmm5\n\t"                                          \
    "movdqu 1*16(%[buf]), %%xmm2\n\t"                                          \
    be_to_le("pshufb %%xmm15, %%xmm5\n\t") /* be => le */                      \
    be_to_le("pshufb %%xmm15, %%xmm2\n\t") /* be => le */                      \
    "pxor %%xmm5, %%xmm1\n\t"                                                  \
                                                                               \
    "pshufd $78, %%xmm10, %%xmm5\n\t"                                          \
    "pshufd $78, %%xmm1, %%xmm4\n\t"                                           \
    "pxor %%xmm10, %%xmm5\n\t" /* xmm5 holds 8:a0+a1 */                        \
    "pxor %%xmm1, %%xmm4\n\t"  /* xmm4 holds 8:b0+b1 */                        \
    "movdqa %%xmm10, %%xmm3\n\t"                                               \
    "pclmulqdq $0, %%xmm1, %%xmm3\n\t"   /* xmm3 holds 8:a0*b0 */              \
    "pclmulqdq $17, %%xmm10, %%xmm1\n\t" /* xmm1 holds 8:a1*b1 */              \
    "pclmulqdq $0, %%xmm5, %%xmm4\n\t"   /* xmm4 holds 8:(a0+a1)*(b0+b1) */    \
                                                                               \
    "pshufd $78, %%xmm9, %%xmm11\n\t"                                          \
    "pshufd $78, %%xmm2, %%xmm7\n\t"                                           \
    "pxor %%xmm9, %%xmm11\n\t" /* xmm11 holds 7:a0+a1 */                       \
    "pxor %%xmm2, %%xmm7\n\t"  /* xmm7 holds 7:b0+b1 */                        \
    "movdqa %%xmm9, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm2, %%xmm6\n\t"  /* xmm6 holds 7:a0*b0 */               \
    "pclmulqdq $17, %%xmm9, %%xmm2\n\t" /* xmm2 holds 7:a1*b1 */               \
    "pclmulqdq $0, %%xmm11, %%xmm7\n\t" /* xmm7 holds 7:(a0+a1)*(b0+b1) */     \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 7+8:a0*b0 */                       \
    "pxor %%xmm2, %%xmm1\n\t" /* xmm1 holds 7+8:a1*b1 */                       \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 7+8:(a0+a1)*(b0+b1) */             \
                                                                               \
    "movdqu 2*16(%[buf]), %%xmm5\n\t"                                          \
    "movdqu 3*16(%[buf]), %%xmm2\n\t"                                          \
    be_to_le("pshufb %%xmm15, %%xmm5\n\t") /* be => le */                      \
    be_to_le("pshufb %%xmm15, %%xmm2\n\t") /* be => le */                      \
                                                                               \
    "pshufd $78, %%xmm8, %%xmm11\n\t"                                          \
    "pshufd $78, %%xmm5, %%xmm7\n\t"                                           \
    "pxor %%xmm8, %%xmm11\n\t" /* xmm11 holds 6:a0+a1 */                       \
    "pxor %%xmm5, %%xmm7\n\t"  /* xmm7 holds 6:b0+b1 */                        \
    "movdqa %%xmm8, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm5, %%xmm6\n\t"  /* xmm6 holds 6:a0*b0 */               \
    "pclmulqdq $17, %%xmm8, %%xmm5\n\t" /* xmm5 holds 6:a1*b1 */               \
    "pclmulqdq $0, %%xmm11, %%xmm7\n\t" /* xmm7 holds 6:(a0+a1)*(b0+b1) */     \
                                                                               \
    /* Load H3, H4, H5. */                                                     \
    "movdqu 3*16(%[h_table]), %%xmm10\n\t"                                     \
    "movdqu 2*16(%[h_table]), %%xmm9\n\t"                                      \
    "movdqu 1*16(%[h_table]), %%xmm8\n\t"                                      \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 6+7+8:a0*b0 */                     \
    "pxor %%xmm5, %%xmm1\n\t" /* xmm1 holds 6+7+8:a1*b1 */                     \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 6+7+8:(a0+a1)*(b0+b1) */           \
                                                                               \
    "pshufd $78, %%xmm10, %%xmm11\n\t"                                         \
    "pshufd $78, %%xmm2, %%xmm7\n\t"                                           \
    "pxor %%xmm10, %%xmm11\n\t" /* xmm11 holds 5:a0+a1 */                      \
    "pxor %%xmm2, %%xmm7\n\t"   /* xmm7 holds 5:b0+b1 */                       \
    "movdqa %%xmm10, %%xmm6\n\t"                                               \
    "pclmulqdq $0, %%xmm2, %%xmm6\n\t"   /* xmm6 holds 5:a0*b0 */              \
    "pclmulqdq $17, %%xmm10, %%xmm2\n\t" /* xmm2 holds 5:a1*b1 */              \
    "pclmulqdq $0, %%xmm11, %%xmm7\n\t"  /* xmm7 holds 5:(a0+a1)*(b0+b1) */    \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 5+6+7+8:a0*b0 */                   \
    "pxor %%xmm2, %%xmm1\n\t" /* xmm1 holds 5+6+7+8:a1*b1 */                   \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 5+6+7+8:(a0+a1)*(b0+b1) */         \
                                                                               \
    "movdqu 4*16(%[buf]), %%xmm5\n\t"                                          \
    "movdqu 5*16(%[buf]), %%xmm2\n\t"                                          \
    be_to_le("pshufb %%xmm15, %%xmm5\n\t") /* be => le */                      \
    be_to_le("pshufb %%xmm15, %%xmm2\n\t") /* be => le */                      \
                                                                               \
    "pshufd $78, %%xmm9, %%xmm11\n\t"                                          \
    "pshufd $78, %%xmm5, %%xmm7\n\t"                                           \
    "pxor %%xmm9, %%xmm11\n\t" /* xmm11 holds 4:a0+a1 */                       \
    "pxor %%xmm5, %%xmm7\n\t"  /* xmm7 holds 4:b0+b1 */                        \
    "movdqa %%xmm9, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm5, %%xmm6\n\t"  /* xmm6 holds 4:a0*b0 */               \
    "pclmulqdq $17, %%xmm9, %%xmm5\n\t" /* xmm5 holds 4:a1*b1 */               \
    "pclmulqdq $0, %%xmm11, %%xmm7\n\t" /* xmm7 holds 4:(a0+a1)*(b0+b1) */     \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 4+5+6+7+8:a0*b0 */                 \
    "pxor %%xmm5, %%xmm1\n\t" /* xmm1 holds 4+5+6+7+8:a1*b1 */                 \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 4+5+6+7+8:(a0+a1)*(b0+b1) */       \
                                                                               \
    "pshufd $78, %%xmm8, %%xmm11\n\t"                                          \
    "pshufd $78, %%xmm2, %%xmm7\n\t"                                           \
    "pxor %%xmm8, %%xmm11\n\t" /* xmm11 holds 3:a0+a1 */                       \
    "pxor %%xmm2, %%xmm7\n\t"  /* xmm7 holds 3:b0+b1 */                        \
    "movdqa %%xmm8, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm2, %%xmm6\n\t"  /* xmm6 holds 3:a0*b0 */               \
    "pclmulqdq $17, %%xmm8, %%xmm2\n\t" /* xmm2 holds 3:a1*b1 */               \
    "pclmulqdq $0, %%xmm11, %%xmm7\n\t" /* xmm7 holds 3:(a0+a1)*(b0+b1) */     \
                                                                               \
    "movdqu 0*16(%[h_table]), %%xmm8\n\t" /* Load H2 */                        \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 3+4+5+6+7+8:a0*b0 */               \
    "pxor %%xmm2, %%xmm1\n\t" /* xmm1 holds 3+4+5+6+7+8:a1*b1 */               \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 3+4+5+6+7+8:(a0+a1)*(b0+b1) */     \
                                                                               \
    "movdqu 6*16(%[buf]), %%xmm5\n\t"                                          \
    "movdqu 7*16(%[buf]), %%xmm2\n\t"                                          \
    be_to_le("pshufb %%xmm15, %%xmm5\n\t") /* be => le */                      \
    be_to_le("pshufb %%xmm15, %%xmm2\n\t") /* be => le */                      \
                                                                               \
    "pshufd $78, %%xmm8, %%xmm11\n\t"                                          \
    "pshufd $78, %%xmm5, %%xmm7\n\t"                                           \
    "pxor %%xmm8, %%xmm11\n\t"  /* xmm11 holds 2:a0+a1 */                      \
    "pxor %%xmm5, %%xmm7\n\t"   /* xmm7 holds 2:b0+b1 */                       \
    "movdqa %%xmm8, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm5, %%xmm6\n\t"   /* xmm6 holds 2:a0*b0 */              \
    "pclmulqdq $17, %%xmm8, %%xmm5\n\t"  /* xmm5 holds 2:a1*b1 */              \
    "pclmulqdq $0, %%xmm11, %%xmm7\n\t"  /* xmm7 holds 2:(a0+a1)*(b0+b1) */    \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 2+3+4+5+6+7+8:a0*b0 */             \
    "pxor %%xmm5, %%xmm1\n\t" /* xmm1 holds 2+3+4+5+6+7+8:a1*b1 */             \
    "pxor %%xmm7, %%xmm4\n\t" /* xmm4 holds 2+3+4+5+6+7+8:(a0+a1)*(b0+b1) */   \
                                                                               \
    "pshufd $78, %%xmm0, %%xmm11\n\t"                                          \
    "pshufd $78, %%xmm2, %%xmm7\n\t"                                           \
    "pxor %%xmm0, %%xmm11\n\t" /* xmm11 holds 1:a0+a1 */                       \
    "pxor %%xmm2, %%xmm7\n\t"  /* xmm7 holds 1:b0+b1 */                        \
    "movdqa %%xmm0, %%xmm6\n\t"                                                \
    "pclmulqdq $0, %%xmm2, %%xmm6\n\t"  /* xmm6 holds 1:a0*b0 */               \
    "pclmulqdq $17, %%xmm0, %%xmm2\n\t" /* xmm2 holds 1:a1*b1 */               \
    "pclmulqdq $0, %%xmm11, %%xmm7\n\t" /* xmm7 holds 1:(a0+a1)*(b0+b1) */     \
                                                                               \
    "pxor %%xmm6, %%xmm3\n\t" /* xmm3 holds 1+2+3+4+5+6+7+8:a0*b0 */           \
    "pxor %%xmm2, %%xmm1\n\t" /* xmm1 holds 1+2+3+4+5+6+7+8:a1*b1 */           \
    "pxor %%xmm7, %%xmm4\n\t"/* xmm4 holds 1+2+3+4+5+6+7+8:(a0+a1)*(b0+b1) */  \
                                                                               \
    /* aggregated reduction... */                                              \
    "movdqa %%xmm3, %%xmm5\n\t"                                                \
    "pxor %%xmm1, %%xmm5\n\t" /* xmm5 holds a0*b0+a1*b1 */                     \
    "pxor %%xmm5, %%xmm4\n\t" /* xmm4 holds a0*b0+a1*b1+(a0+a1)*(b0+b1) */     \
    "movdqa %%xmm4, %%xmm5\n\t"                                                \
    "psrldq $8, %%xmm4\n\t"                                                    \
    "pslldq $8, %%xmm5\n\t"                                                    \
    "pxor %%xmm5, %%xmm3\n\t"                                                  \
    "pxor %%xmm4, %%xmm1\n\t" /* <xmm1:xmm3> holds the result of the           \
                                  carry-less multiplication of xmm0            \
                                  by xmm1 */

static ASM_FUNC_ATTR_INLINE void
gfmul_pclmul_aggr8(const void *buf, const void *h_table)
{
  /* Input:
      H¹: XMM0
      bemask: XMM15
      Hash: XMM1
     Output:
      Hash: XMM1
     Inputs XMM0 and XMM15 stays unmodified.
   */
  asm volatile (GFMUL_AGGR8_ASM(be_to_le)
                :
                : [buf] "r" (buf),
                  [h_table] "r" (h_table)
                : "memory" );

  reduction();
}

static ASM_FUNC_ATTR_INLINE void
gfmul_pclmul_aggr8_le(const void *buf, const void *h_table)
{
  /* Input:
      H¹: XMM0
      Hash: XMM1
     Output:
      Hash: XMM1
     Inputs XMM0 and XMM15 stays unmodified.
   */
  asm volatile (GFMUL_AGGR8_ASM(le_to_le)
                :
                : [buf] "r" (buf),
                  [h_table] "r" (h_table)
                : "memory" );

  reduction();
}

#ifdef GCM_USE_INTEL_VPCLMUL_AVX2

#define GFMUL_AGGR16_ASM_VPCMUL_AVX2(be_to_le)                                          \
    /* perform clmul and merge results... */                                            \
    "vmovdqu 0*16(%[buf]), %%ymm5\n\t"                                                  \
    "vmovdqu 2*16(%[buf]), %%ymm2\n\t"                                                  \
    be_to_le("vpshufb %%ymm15, %%ymm5, %%ymm5\n\t") /* be => le */                      \
    be_to_le("vpshufb %%ymm15, %%ymm2, %%ymm2\n\t") /* be => le */                      \
    "vpxor %%ymm5, %%ymm1, %%ymm1\n\t"                                                  \
                                                                                        \
    "vpshufd $78, %%ymm0, %%ymm5\n\t"                                                   \
    "vpshufd $78, %%ymm1, %%ymm4\n\t"                                                   \
    "vpxor %%ymm0, %%ymm5, %%ymm5\n\t" /* ymm5 holds 15|16:a0+a1 */                     \
    "vpxor %%ymm1, %%ymm4, %%ymm4\n\t" /* ymm4 holds 15|16:b0+b1 */                     \
    "vpclmulqdq $0, %%ymm1, %%ymm0, %%ymm3\n\t"  /* ymm3 holds 15|16:a0*b0 */           \
    "vpclmulqdq $17, %%ymm0, %%ymm1, %%ymm1\n\t" /* ymm1 holds 15|16:a1*b1 */           \
    "vpclmulqdq $0, %%ymm5, %%ymm4, %%ymm4\n\t"  /* ymm4 holds 15|16:(a0+a1)*(b0+b1) */ \
                                                                                        \
    "vmovdqu %[h1_h2], %%ymm0\n\t"                                                      \
                                                                                        \
    "vpshufd $78, %%ymm13, %%ymm14\n\t"                                                 \
    "vpshufd $78, %%ymm2, %%ymm7\n\t"                                                   \
    "vpxor %%ymm13, %%ymm14, %%ymm14\n\t" /* ymm14 holds 13|14:a0+a1 */                 \
    "vpxor %%ymm2, %%ymm7, %%ymm7\n\t"    /* ymm7 holds 13|14:b0+b1 */                  \
    "vpclmulqdq $0, %%ymm2, %%ymm13, %%ymm6\n\t"  /* ymm6 holds 13|14:a0*b0 */          \
    "vpclmulqdq $17, %%ymm13, %%ymm2, %%ymm2\n\t" /* ymm2 holds 13|14:a1*b1 */          \
    "vpclmulqdq $0, %%ymm14, %%ymm7, %%ymm7\n\t"  /* ymm7 holds 13|14:(a0+a1)*(b0+b1) */\
                                                                                        \
    "vpxor %%ymm6, %%ymm3, %%ymm3\n\t" /* ymm3 holds 13+15|14+16:a0*b0 */               \
    "vpxor %%ymm2, %%ymm1, %%ymm1\n\t" /* ymm1 holds 13+15|14+16:a1*b1 */               \
    "vpxor %%ymm7, %%ymm4, %%ymm4\n\t" /* ymm4 holds 13+15|14+16:(a0+a1)*(b0+b1) */     \
                                                                                        \
    "vmovdqu 4*16(%[buf]), %%ymm5\n\t"                                                  \
    "vmovdqu 6*16(%[buf]), %%ymm2\n\t"                                                  \
    be_to_le("vpshufb %%ymm15, %%ymm5, %%ymm5\n\t") /* be => le */                      \
    be_to_le("vpshufb %%ymm15, %%ymm2, %%ymm2\n\t") /* be => le */                      \
                                                                                        \
    "vpshufd $78, %%ymm12, %%ymm14\n\t"                                                 \
    "vpshufd $78, %%ymm5, %%ymm7\n\t"                                                   \
    "vpxor %%ymm12, %%ymm14, %%ymm14\n\t" /* ymm14 holds 11|12:a0+a1 */                 \
    "vpxor %%ymm5, %%ymm7, %%ymm7\n\t"    /* ymm7 holds 11|12:b0+b1 */                  \
    "vpclmulqdq $0, %%ymm5, %%ymm12, %%ymm6\n\t"  /* ymm6 holds 11|12:a0*b0 */          \
    "vpclmulqdq $17, %%ymm12, %%ymm5, %%ymm5\n\t" /* ymm5 holds 11|12:a1*b1 */          \
    "vpclmulqdq $0, %%ymm14, %%ymm7, %%ymm7\n\t"  /* ymm7 holds 11|12:(a0+a1)*(b0+b1) */\
                                                                                        \
    "vpxor %%ymm6, %%ymm3, %%ymm3\n\t" /* ymm3 holds 11+13+15|12+14+16:a0*b0 */         \
    "vpxor %%ymm5, %%ymm1, %%ymm1\n\t" /* ymm1 holds 11+13+15|12+14+16:a1*b1 */         \
    "vpxor %%ymm7, %%ymm4, %%ymm4\n\t" /* ymm4 holds 11+13+15|12+14+16:(a0+a1)*(b0+b1) */\
                                                                                        \
    "vpshufd $78, %%ymm11, %%ymm14\n\t"                                                 \
    "vpshufd $78, %%ymm2, %%ymm7\n\t"                                                   \
    "vpxor %%ymm11, %%ymm14, %%ymm14\n\t" /* ymm14 holds 9|10:a0+a1 */                  \
    "vpxor %%ymm2, %%ymm7, %%ymm7\n\t"    /* ymm7 holds 9|10:b0+b1 */                   \
    "vpclmulqdq $0, %%ymm2, %%ymm11, %%ymm6\n\t"  /* ymm6 holds 9|10:a0*b0 */           \
    "vpclmulqdq $17, %%ymm11, %%ymm2, %%ymm2\n\t" /* ymm2 holds 9|10:a1*b1 */           \
    "vpclmulqdq $0, %%ymm14, %%ymm7, %%ymm7\n\t" /* ymm7 holds 9|10:(a0+a1)*(b0+b1) */  \
                                                                                        \
    "vpxor %%ymm6, %%ymm3, %%ymm3\n\t" /* ymm3 holds 9+11+…+15|10+12+…+16:a0*b0 */      \
    "vpxor %%ymm2, %%ymm1, %%ymm1\n\t" /* ymm1 holds 9+11+…+15|10+12+…+16:a1*b1 */      \
    "vpxor %%ymm7, %%ymm4, %%ymm4\n\t" /* ymm4 holds 9+11+…+15|10+12+…+16:(a0+a1)*(b0+b1) */\
                                                                                        \
    "vmovdqu 8*16(%[buf]), %%ymm5\n\t"                                                  \
    "vmovdqu 10*16(%[buf]), %%ymm2\n\t"                                                 \
    be_to_le("vpshufb %%ymm15, %%ymm5, %%ymm5\n\t") /* be => le */                      \
    be_to_le("vpshufb %%ymm15, %%ymm2, %%ymm2\n\t") /* be => le */                      \
                                                                                        \
    "vpshufd $78, %%ymm10, %%ymm14\n\t"                                                 \
    "vpshufd $78, %%ymm5, %%ymm7\n\t"                                                   \
    "vpxor %%ymm10, %%ymm14, %%ymm14\n\t" /* ymm14 holds 7|8:a0+a1 */                   \
    "vpxor %%ymm5, %%ymm7, %%ymm7\n\t"    /* ymm7 holds 7|8:b0+b1 */                    \
    "vpclmulqdq $0, %%ymm5, %%ymm10, %%ymm6\n\t"  /* ymm6 holds 7|8:a0*b0 */            \
    "vpclmulqdq $17, %%ymm10, %%ymm5, %%ymm5\n\t" /* ymm5 holds 7|8:a1*b1 */            \
    "vpclmulqdq $0, %%ymm14, %%ymm7, %%ymm7\n\t" /* ymm7 holds 7|8:(a0+a1)*(b0+b1) */   \
                                                                                        \
    "vpxor %%ymm6, %%ymm3, %%ymm3\n\t" /* ymm3 holds 7+9+…+15|8+10+…+16:a0*b0 */        \
    "vpxor %%ymm5, %%ymm1, %%ymm1\n\t" /* ymm1 holds 7+9+…+15|8+10+…+16:a1*b1 */        \
    "vpxor %%ymm7, %%ymm4, %%ymm4\n\t" /* ymm4 holds 7+9+…+15|8+10+…+16:(a0+a1)*(b0+b1) */\
                                                                                        \
    "vpshufd $78, %%ymm9, %%ymm14\n\t"                                                  \
    "vpshufd $78, %%ymm2, %%ymm7\n\t"                                                   \
    "vpxor %%ymm9, %%ymm14, %%ymm14\n\t" /* ymm14 holds 5|6:a0+a1 */                    \
    "vpxor %%ymm2, %%ymm7, %%ymm7\n\t"   /* ymm7 holds 5|6:b0+b1 */                     \
    "vpclmulqdq $0, %%ymm2, %%ymm9, %%ymm6\n\t"  /* ymm6 holds 5|6:a0*b0 */             \
    "vpclmulqdq $17, %%ymm9, %%ymm2, %%ymm2\n\t" /* ymm2 holds 5|6:a1*b1 */             \
    "vpclmulqdq $0, %%ymm14, %%ymm7, %%ymm7\n\t" /* ymm7 holds 5|6:(a0+a1)*(b0+b1) */   \
                                                                                        \
    "vpxor %%ymm6, %%ymm3, %%ymm3\n\t" /* ymm3 holds 5+7+…+15|6+8+…+16:a0*b0 */         \
    "vpxor %%ymm2, %%ymm1, %%ymm1\n\t" /* ymm1 holds 5+7+…+15|6+8+…+16:a1*b1 */         \
    "vpxor %%ymm7, %%ymm4, %%ymm4\n\t" /* ymm4 holds 5+7+…+15|6+8+…+16:(a0+a1)*(b0+b1) */\
                                                                                        \
    "vmovdqu 12*16(%[buf]), %%ymm5\n\t"                                                 \
    "vmovdqu 14*16(%[buf]), %%ymm2\n\t"                                                 \
    be_to_le("vpshufb %%ymm15, %%ymm5, %%ymm5\n\t") /* be => le */                      \
    be_to_le("vpshufb %%ymm15, %%ymm2, %%ymm2\n\t") /* be => le */                      \
                                                                                        \
    "vpshufd $78, %%ymm8, %%ymm14\n\t"                                                  \
    "vpshufd $78, %%ymm5, %%ymm7\n\t"                                                   \
    "vpxor %%ymm8, %%ymm14, %%ymm14\n\t" /* ymm14 holds 3|4:a0+a1 */                    \
    "vpxor %%ymm5, %%ymm7, %%ymm7\n\t"   /* ymm7 holds 3|4:b0+b1 */                     \
    "vpclmulqdq $0, %%ymm5, %%ymm8, %%ymm6\n\t"  /* ymm6 holds 3|4:a0*b0 */             \
    "vpclmulqdq $17, %%ymm8, %%ymm5, %%ymm5\n\t" /* ymm5 holds 3|4:a1*b1 */             \
    "vpclmulqdq $0, %%ymm14, %%ymm7, %%ymm7\n\t" /* ymm7 holds 3|4:(a0+a1)*(b0+b1) */   \
                                                                                        \
    "vpxor %%ymm6, %%ymm3, %%ymm3\n\t" /* ymm3 holds 3+5+…+15|4+6+…+16:a0*b0 */         \
    "vpxor %%ymm5, %%ymm1, %%ymm1\n\t" /* ymm1 holds 3+5+…+15|4+6+…+16:a1*b1 */         \
    "vpxor %%ymm7, %%ymm4, %%ymm4\n\t" /* ymm4 holds 3+5+…+15|4+6+…+16:(a0+a1)*(b0+b1) */\
                                                                                        \
    "vpshufd $78, %%ymm0, %%ymm14\n\t"                                                  \
    "vpshufd $78, %%ymm2, %%ymm7\n\t"                                                   \
    "vpxor %%ymm0, %%ymm14, %%ymm14\n\t" /* ymm14 holds 1|2:a0+a1 */                    \
    "vpxor %%ymm2, %%ymm7, %%ymm7\n\t"   /* ymm7 holds 1|2:b0+b1 */                     \
    "vpclmulqdq $0, %%ymm2, %%ymm0, %%ymm6\n\t"  /* ymm6 holds 1|2:a0*b0 */             \
    "vpclmulqdq $17, %%ymm0, %%ymm2, %%ymm2\n\t" /* ymm2 holds 1|2:a1*b1 */             \
    "vpclmulqdq $0, %%ymm14, %%ymm7, %%ymm7\n\t" /* ymm7 holds 1|2:(a0+a1)*(b0+b1) */   \
                                                                                        \
    "vmovdqu %[h15_h16], %%ymm0\n\t"                                                    \
                                                                                        \
    "vpxor %%ymm6, %%ymm3, %%ymm3\n\t" /* ymm3 holds 1+3+…+15|2+4+…+16:a0*b0 */         \
    "vpxor %%ymm2, %%ymm1, %%ymm1\n\t" /* ymm1 holds 1+3+…+15|2+4+…+16:a1*b1 */         \
    "vpxor %%ymm7, %%ymm4, %%ymm4\n\t" /* ymm4 holds 1+3+…+15|2+4+…+16:(a0+a1)*(b0+b1) */\
                                                                                        \
    /* aggregated reduction... */                                                       \
    "vpxor %%ymm1, %%ymm3, %%ymm5\n\t" /* ymm5 holds a0*b0+a1*b1 */                     \
    "vpxor %%ymm5, %%ymm4, %%ymm4\n\t" /* ymm4 holds a0*b0+a1*b1+(a0+a1)*(b0+b1) */     \
    "vpslldq $8, %%ymm4, %%ymm5\n\t"                                                    \
    "vpsrldq $8, %%ymm4, %%ymm4\n\t"                                                    \
    "vpxor %%ymm5, %%ymm3, %%ymm3\n\t"                                                  \
    "vpxor %%ymm4, %%ymm1, %%ymm1\n\t" /* <ymm1:xmm3> holds the result of the           \
                                          carry-less multiplication of ymm0             \
                                          by ymm1 */                                    \
                                                                                        \
    /* first phase of the reduction */                                                  \
    "vpsllq $1, %%ymm3, %%ymm6\n\t"  /* packed right shifting << 63 */                  \
    "vpxor %%ymm3, %%ymm6, %%ymm6\n\t"                                                  \
    "vpsllq $57, %%ymm3, %%ymm5\n\t"  /* packed right shifting << 57 */                 \
    "vpsllq $62, %%ymm6, %%ymm6\n\t"  /* packed right shifting << 62 */                 \
    "vpxor %%ymm5, %%ymm6, %%ymm6\n\t" /* xor the shifted versions */                   \
    "vpshufd $0x6a, %%ymm6, %%ymm5\n\t"                                                 \
    "vpshufd $0xae, %%ymm6, %%ymm6\n\t"                                                 \
    "vpxor %%ymm5, %%ymm3, %%ymm3\n\t" /* first phase of the reduction complete */      \
                                                                                        \
    /* second phase of the reduction */                                                 \
    "vpxor %%ymm3, %%ymm1, %%ymm1\n\t" /* xor the shifted versions */                   \
    "vpsrlq $1, %%ymm3, %%ymm3\n\t"    /* packed left shifting >> 1 */                  \
    "vpxor %%ymm3, %%ymm6, %%ymm6\n\t"                                                  \
    "vpsrlq $1, %%ymm3, %%ymm3\n\t"    /* packed left shifting >> 2 */                  \
    "vpxor %%ymm3, %%ymm1, %%ymm1\n\t"                                                  \
    "vpsrlq $5, %%ymm3, %%ymm3\n\t"    /* packed left shifting >> 7 */                  \
    "vpxor %%ymm3, %%ymm6, %%ymm6\n\t"                                                  \
    "vpxor %%ymm6, %%ymm1, %%ymm1\n\t" /* the result is in ymm1 */                      \
                                                                                        \
    /* merge 128-bit halves */                                                          \
    "vextracti128 $1, %%ymm1, %%xmm2\n\t"                                               \
    "vpxor %%xmm2, %%xmm1, %%xmm1\n\t"

static ASM_FUNC_ATTR_INLINE void
gfmul_vpclmul_avx2_aggr16(const void *buf, const void *h_table,
			  const u64 *h1_h2_h15_h16)
{
  /* Input:
      Hx: YMM0, YMM8, YMM9, YMM10, YMM11, YMM12, YMM13
      bemask: YMM15
      Hash: XMM1
    Output:
      Hash: XMM1
    Inputs YMM0, YMM8, YMM9, YMM10, YMM11, YMM12, YMM13 and YMM15 stay
    unmodified.
  */
  asm volatile (GFMUL_AGGR16_ASM_VPCMUL_AVX2(be_to_le)
		:
		: [buf] "r" (buf),
		  [h_table] "r" (h_table),
		  [h1_h2] "m" (h1_h2_h15_h16[0]),
		  [h15_h16] "m" (h1_h2_h15_h16[4])
		: "memory" );
}

static ASM_FUNC_ATTR_INLINE void
gfmul_vpclmul_avx2_aggr16_le(const void *buf, const void *h_table,
			     const u64 *h1_h2_h15_h16)
{
  /* Input:
      Hx: YMM0, YMM8, YMM9, YMM10, YMM11, YMM12, YMM13
      bemask: YMM15
      Hash: XMM1
    Output:
      Hash: XMM1
    Inputs YMM0, YMM8, YMM9, YMM10, YMM11, YMM12, YMM13 and YMM15 stay
    unmodified.
  */
  asm volatile (GFMUL_AGGR16_ASM_VPCMUL_AVX2(le_to_le)
		:
		: [buf] "r" (buf),
		  [h_table] "r" (h_table),
		  [h1_h2] "m" (h1_h2_h15_h16[0]),
		  [h15_h16] "m" (h1_h2_h15_h16[4])
		: "memory" );
}

static ASM_FUNC_ATTR_INLINE
void gfmul_pclmul_avx2(void)
{
  /* Input: YMM0 and YMM1, Output: YMM1. Input YMM0 stays unmodified.
     Input must be converted to little-endian.
   */
  asm volatile (/* gfmul, ymm0 has operator a and ymm1 has operator b. */
		"vpshufd $78, %%ymm0, %%ymm2\n\t"
		"vpshufd $78, %%ymm1, %%ymm4\n\t"
		"vpxor %%ymm0, %%ymm2, %%ymm2\n\t" /* ymm2 holds a0+a1 */
		"vpxor %%ymm1, %%ymm4, %%ymm4\n\t" /* ymm4 holds b0+b1 */

		"vpclmulqdq $0, %%ymm1, %%ymm0, %%ymm3\n\t"  /* ymm3 holds a0*b0 */
		"vpclmulqdq $17, %%ymm0, %%ymm1, %%ymm1\n\t" /* ymm6 holds a1*b1 */
		"vpclmulqdq $0, %%ymm2, %%ymm4, %%ymm4\n\t"  /* ymm4 holds (a0+a1)*(b0+b1) */

		"vpxor %%ymm1, %%ymm3, %%ymm5\n\t" /* ymm5 holds a0*b0+a1*b1 */
		"vpxor %%ymm5, %%ymm4, %%ymm4\n\t" /* ymm4 holds a0*b0+a1*b1+(a0+a1)*(b0+b1) */
		"vpslldq $8, %%ymm4, %%ymm5\n\t"
		"vpsrldq $8, %%ymm4, %%ymm4\n\t"
		"vpxor %%ymm5, %%ymm3, %%ymm3\n\t"
		"vpxor %%ymm4, %%ymm1, %%ymm1\n\t" /* <ymm1:ymm3> holds the result of the
						      carry-less multiplication of ymm0
						      by ymm1 */

		/* first phase of the reduction */
		"vpsllq $1, %%ymm3, %%ymm6\n\t"  /* packed right shifting << 63 */
		"vpxor %%ymm3, %%ymm6, %%ymm6\n\t"
		"vpsllq $57, %%ymm3, %%ymm5\n\t"  /* packed right shifting << 57 */
		"vpsllq $62, %%ymm6, %%ymm6\n\t"  /* packed right shifting << 62 */
		"vpxor %%ymm5, %%ymm6, %%ymm6\n\t" /* xor the shifted versions */
		"vpshufd $0x6a, %%ymm6, %%ymm5\n\t"
		"vpshufd $0xae, %%ymm6, %%ymm6\n\t"
		"vpxor %%ymm5, %%ymm3, %%ymm3\n\t" /* first phase of the reduction complete */

		/* second phase of the reduction */
		"vpxor %%ymm3, %%ymm1, %%ymm1\n\t" /* xor the shifted versions */
		"vpsrlq $1, %%ymm3, %%ymm3\n\t"    /* packed left shifting >> 1 */
		"vpxor %%ymm3, %%ymm6, %%ymm6\n\t"
		"vpsrlq $1, %%ymm3, %%ymm3\n\t"    /* packed left shifting >> 2 */
		"vpxor %%ymm3, %%ymm1, %%ymm1\n\t"
		"vpsrlq $5, %%ymm3, %%ymm3\n\t"    /* packed left shifting >> 7 */
		"vpxor %%ymm3, %%ymm6, %%ymm6\n\t"
		"vpxor %%ymm6, %%ymm1, %%ymm1\n\t" /* the result is in ymm1 */
                ::: "memory" );
}

static ASM_FUNC_ATTR_INLINE void
gcm_lsh_avx2(void *h, unsigned int hoffs)
{
  static const u64 pconst[4] __attribute__ ((aligned (32))) =
    {
      U64_C(0x0000000000000001), U64_C(0xc200000000000000),
      U64_C(0x0000000000000001), U64_C(0xc200000000000000)
    };

  asm volatile ("vmovdqu %[h], %%ymm2\n\t"
                "vpshufd $0xff, %%ymm2, %%ymm3\n\t"
                "vpsrad $31, %%ymm3, %%ymm3\n\t"
                "vpslldq $8, %%ymm2, %%ymm4\n\t"
                "vpand %[pconst], %%ymm3, %%ymm3\n\t"
                "vpaddq %%ymm2, %%ymm2, %%ymm2\n\t"
                "vpsrlq $63, %%ymm4, %%ymm4\n\t"
                "vpxor %%ymm3, %%ymm2, %%ymm2\n\t"
                "vpxor %%ymm4, %%ymm2, %%ymm2\n\t"
                "vmovdqu %%ymm2, %[h]\n\t"
                : [h] "+m" (*((byte *)h + hoffs))
                : [pconst] "m" (*pconst)
                : "memory" );
}

static ASM_FUNC_ATTR_INLINE void
load_h1h2_to_ymm1(gcry_cipher_hd_t c)
{
  unsigned int key_pos =
    offsetof(struct gcry_cipher_handle, u_mode.gcm.u_ghash_key.key);
  unsigned int table_pos =
    offsetof(struct gcry_cipher_handle, u_mode.gcm.gcm_table);

  if (key_pos + 16 == table_pos)
    {
      /* Optimization: Table follows immediately after key. */
      asm volatile ("vmovdqu %[key], %%ymm1\n\t"
		    :
		    : [key] "m" (*c->u_mode.gcm.u_ghash_key.key)
		    : "memory");
    }
  else
    {
      asm volatile ("vmovdqa %[key], %%xmm1\n\t"
		    "vinserti128 $1, 0*16(%[h_table]), %%ymm1, %%ymm1\n\t"
		    :
		    : [h_table] "r" (c->u_mode.gcm.gcm_table),
		      [key] "m" (*c->u_mode.gcm.u_ghash_key.key)
		    : "memory");
    }
}

static ASM_FUNC_ATTR void
ghash_setup_aggr8_avx2(gcry_cipher_hd_t c)
{
  c->u_mode.gcm.hw_impl_flags |= GCM_INTEL_AGGR8_TABLE_INITIALIZED;

  asm volatile (/* load H⁴ */
		"vbroadcasti128 3*16(%[h_table]), %%ymm0\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");
  /* load H <<< 1, H² <<< 1 */
  load_h1h2_to_ymm1 (c);

  gfmul_pclmul_avx2 (); /* H<<<1•H⁴ => H⁵, H²<<<1•H⁴ => H⁶ */

  asm volatile ("vmovdqu %%ymm1, 3*16(%[h_table])\n\t"
		/* load H³ <<< 1, H⁴ <<< 1 */
		"vmovdqu 1*16(%[h_table]), %%ymm1\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul_avx2 (); /* H³<<<1•H⁴ => H⁷, H⁴<<<1•H⁴ => H⁸ */

  asm volatile ("vmovdqu %%ymm1, 6*16(%[h_table])\n\t" /* store H⁸ for aggr16 setup */
		"vmovdqu %%ymm1, 5*16(%[h_table])\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gcm_lsh_avx2 (c->u_mode.gcm.gcm_table, 3 * 16); /* H⁵ <<< 1, H⁶ <<< 1 */
  gcm_lsh_avx2 (c->u_mode.gcm.gcm_table, 5 * 16); /* H⁷ <<< 1, H⁸ <<< 1 */
}

static ASM_FUNC_ATTR void
ghash_setup_aggr16_avx2(gcry_cipher_hd_t c)
{
  c->u_mode.gcm.hw_impl_flags |= GCM_INTEL_AGGR16_TABLE_INITIALIZED;

  asm volatile (/* load H⁸ */
		"vbroadcasti128 7*16(%[h_table]), %%ymm0\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");
  /* load H <<< 1, H² <<< 1 */
  load_h1h2_to_ymm1 (c);

  gfmul_pclmul_avx2 (); /* H<<<1•H⁸ => H⁹, H²<<<1•H⁸ => H¹⁰ */

  asm volatile ("vmovdqu %%ymm1, 7*16(%[h_table])\n\t"
		/* load H³ <<< 1, H⁴ <<< 1 */
		"vmovdqu 1*16(%[h_table]), %%ymm1\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul_avx2 (); /* H³<<<1•H⁸ => H¹¹, H⁴<<<1•H⁸ => H¹² */

  asm volatile ("vmovdqu %%ymm1, 9*16(%[h_table])\n\t"
		/* load H⁵ <<< 1, H⁶ <<< 1 */
		"vmovdqu 3*16(%[h_table]), %%ymm1\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul_avx2 (); /* H⁵<<<1•H⁸ => H¹³, H⁶<<<1•H⁸ => H¹⁴ */

  asm volatile ("vmovdqu %%ymm1, 11*16(%[h_table])\n\t"
		/* load H⁷ <<< 1, H⁸ <<< 1 */
		"vmovdqu 5*16(%[h_table]), %%ymm1\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul_avx2 (); /* H⁷<<<1•H⁸ => H¹⁵, H⁸<<<1•H⁸ => H¹⁶ */

  asm volatile ("vmovdqu %%ymm1, 14*16(%[h_table])\n\t" /* store H¹⁶ for aggr32 setup */
                "vmovdqu %%ymm1, 13*16(%[h_table])\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gcm_lsh_avx2 (c->u_mode.gcm.gcm_table, 7 * 16); /* H⁹ <<< 1, H¹⁰ <<< 1 */
  gcm_lsh_avx2 (c->u_mode.gcm.gcm_table, 9 * 16); /* H¹¹ <<< 1, H¹² <<< 1 */
  gcm_lsh_avx2 (c->u_mode.gcm.gcm_table, 11 * 16); /* H¹³ <<< 1, H¹⁴ <<< 1 */
  gcm_lsh_avx2 (c->u_mode.gcm.gcm_table, 13 * 16); /* H¹⁵ <<< 1, H¹⁶ <<< 1 */
}

#endif /* GCM_USE_INTEL_VPCLMUL_AVX2 */

#ifdef GCM_USE_INTEL_VPCLMUL_AVX512

#define GFMUL_AGGR32_ASM_VPCMUL_AVX512(be_to_le)                                          \
    /* perform clmul and merge results... */                                              \
    "vmovdqu64 0*16(%[buf]), %%zmm5\n\t"                                                  \
    "vmovdqu64 4*16(%[buf]), %%zmm2\n\t"                                                  \
    be_to_le("vpshufb %%zmm15, %%zmm5, %%zmm5\n\t") /* be => le */                        \
    be_to_le("vpshufb %%zmm15, %%zmm2, %%zmm2\n\t") /* be => le */                        \
    "vpxorq %%zmm5, %%zmm1, %%zmm1\n\t"                                                   \
                                                                                          \
    "vpshufd $78, %%zmm0, %%zmm5\n\t"                                                     \
    "vpshufd $78, %%zmm1, %%zmm4\n\t"                                                     \
    "vpxorq %%zmm0, %%zmm5, %%zmm5\n\t" /* zmm5 holds 29|…|32:a0+a1 */                    \
    "vpxorq %%zmm1, %%zmm4, %%zmm4\n\t" /* zmm4 holds 29|…|32:b0+b1 */                    \
    "vpclmulqdq $0, %%zmm1, %%zmm0, %%zmm3\n\t"  /* zmm3 holds 29|…|32:a0*b0 */           \
    "vpclmulqdq $17, %%zmm0, %%zmm1, %%zmm1\n\t" /* zmm1 holds 29|…|32:a1*b1 */           \
    "vpclmulqdq $0, %%zmm5, %%zmm4, %%zmm4\n\t"  /* zmm4 holds 29|…|32:(a0+a1)*(b0+b1) */ \
                                                                                          \
    "vpshufd $78, %%zmm13, %%zmm14\n\t"                                                   \
    "vpshufd $78, %%zmm2, %%zmm7\n\t"                                                     \
    "vpxorq %%zmm13, %%zmm14, %%zmm14\n\t" /* zmm14 holds 25|…|28:a0+a1 */                \
    "vpxorq %%zmm2, %%zmm7, %%zmm7\n\t"    /* zmm7 holds 25|…|28:b0+b1 */                 \
    "vpclmulqdq $0, %%zmm2, %%zmm13, %%zmm17\n\t"  /* zmm17 holds 25|…|28:a0*b0 */        \
    "vpclmulqdq $17, %%zmm13, %%zmm2, %%zmm18\n\t" /* zmm18 holds 25|…|28:a1*b1 */        \
    "vpclmulqdq $0, %%zmm14, %%zmm7, %%zmm19\n\t"  /* zmm19 holds 25|…|28:(a0+a1)*(b0+b1) */\
                                                                                          \
    "vmovdqu64 8*16(%[buf]), %%zmm5\n\t"                                                  \
    "vmovdqu64 12*16(%[buf]), %%zmm2\n\t"                                                 \
    be_to_le("vpshufb %%zmm15, %%zmm5, %%zmm5\n\t") /* be => le */                        \
    be_to_le("vpshufb %%zmm15, %%zmm2, %%zmm2\n\t") /* be => le */                        \
                                                                                          \
    "vpshufd $78, %%zmm12, %%zmm14\n\t"                                                   \
    "vpshufd $78, %%zmm5, %%zmm7\n\t"                                                     \
    "vpxorq %%zmm12, %%zmm14, %%zmm14\n\t" /* zmm14 holds 21|…|24:a0+a1 */                \
    "vpxorq %%zmm5, %%zmm7, %%zmm7\n\t"    /* zmm7 holds 21|…|24:b0+b1 */                 \
    "vpclmulqdq $0, %%zmm5, %%zmm12, %%zmm6\n\t"  /* zmm6 holds 21|…|24:a0*b0 */          \
    "vpclmulqdq $17, %%zmm12, %%zmm5, %%zmm5\n\t" /* zmm5 holds 21|…|24:a1*b1 */          \
    "vpclmulqdq $0, %%zmm14, %%zmm7, %%zmm7\n\t"  /* zmm7 holds 21|…|24:(a0+a1)*(b0+b1) */\
                                                                                          \
    "vpternlogq $0x96, %%zmm6, %%zmm17, %%zmm3\n\t" /* zmm3 holds 21+…|…|…+32:a0*b0 */    \
    "vpternlogq $0x96, %%zmm5, %%zmm18, %%zmm1\n\t" /* zmm1 holds 21+…|…|…+32:a1*b1 */    \
    "vpternlogq $0x96, %%zmm7, %%zmm19, %%zmm4\n\t" /* zmm4 holds 21+…|…|…+32:(a0+a1)*(b0+b1) */\
                                                                                          \
    "vpshufd $78, %%zmm11, %%zmm14\n\t"                                                   \
    "vpshufd $78, %%zmm2, %%zmm7\n\t"                                                     \
    "vpxorq %%zmm11, %%zmm14, %%zmm14\n\t" /* zmm14 holds 17|…|20:a0+a1 */                \
    "vpxorq %%zmm2, %%zmm7, %%zmm7\n\t"    /* zmm7 holds 17|…|20:b0+b1 */                 \
    "vpclmulqdq $0, %%zmm2, %%zmm11, %%zmm17\n\t"  /* zmm17 holds 17|…|20:a0*b0 */        \
    "vpclmulqdq $17, %%zmm11, %%zmm2, %%zmm18\n\t" /* zmm18 holds 17|…|20:a1*b1 */        \
    "vpclmulqdq $0, %%zmm14, %%zmm7, %%zmm19\n\t" /* zmm19 holds 17|…|20:(a0+a1)*(b0+b1) */\
                                                                                          \
    "vmovdqu64 16*16(%[buf]), %%zmm5\n\t"                                                 \
    "vmovdqu64 20*16(%[buf]), %%zmm2\n\t"                                                 \
    be_to_le("vpshufb %%zmm15, %%zmm5, %%zmm5\n\t") /* be => le */                        \
    be_to_le("vpshufb %%zmm15, %%zmm2, %%zmm2\n\t") /* be => le */                        \
                                                                                          \
    "vpshufd $78, %%zmm10, %%zmm14\n\t"                                                   \
    "vpshufd $78, %%zmm5, %%zmm7\n\t"                                                     \
    "vpxorq %%zmm10, %%zmm14, %%zmm14\n\t" /* zmm14 holds 13|…|16:a0+a1 */                \
    "vpxorq %%zmm5, %%zmm7, %%zmm7\n\t"    /* zmm7 holds 13|…|16:b0+b1 */                 \
    "vpclmulqdq $0, %%zmm5, %%zmm10, %%zmm6\n\t"  /* zmm6 holds 13|…|16:a0*b0 */          \
    "vpclmulqdq $17, %%zmm10, %%zmm5, %%zmm5\n\t" /* zmm5 holds 13|…|16:a1*b1 */          \
    "vpclmulqdq $0, %%zmm14, %%zmm7, %%zmm7\n\t" /* zmm7 holds 13|…|16:(a0+a1)*(b0+b1) */ \
                                                                                          \
    "vpternlogq $0x96, %%zmm6, %%zmm17, %%zmm3\n\t" /* zmm3 holds 13+…|…|…+32:a0*b0 */    \
    "vpternlogq $0x96, %%zmm5, %%zmm18, %%zmm1\n\t" /* zmm1 holds 13+…|…|…+32:a1*b1 */    \
    "vpternlogq $0x96, %%zmm7, %%zmm19, %%zmm4\n\t" /* zmm4 holds 13+…|…|…+32:(a0+a1)*(b0+b1) */\
                                                                                          \
    "vpshufd $78, %%zmm9, %%zmm14\n\t"                                                    \
    "vpshufd $78, %%zmm2, %%zmm7\n\t"                                                     \
    "vpxorq %%zmm9, %%zmm14, %%zmm14\n\t" /* zmm14 holds 9|…|12:a0+a1 */                  \
    "vpxorq %%zmm2, %%zmm7, %%zmm7\n\t"   /* zmm7 holds 9|…|12:b0+b1 */                   \
    "vpclmulqdq $0, %%zmm2, %%zmm9, %%zmm17\n\t"  /* zmm17 holds 9|…|12:a0*b0 */          \
    "vpclmulqdq $17, %%zmm9, %%zmm2, %%zmm18\n\t" /* zmm18 holds 9|…|12:a1*b1 */          \
    "vpclmulqdq $0, %%zmm14, %%zmm7, %%zmm19\n\t" /* zmm19 holds 9|…|12:(a0+a1)*(b0+b1) */\
                                                                                          \
    "vmovdqu64 24*16(%[buf]), %%zmm5\n\t"                                                 \
    "vmovdqu64 28*16(%[buf]), %%zmm2\n\t"                                                 \
    be_to_le("vpshufb %%zmm15, %%zmm5, %%zmm5\n\t") /* be => le */                        \
    be_to_le("vpshufb %%zmm15, %%zmm2, %%zmm2\n\t") /* be => le */                        \
                                                                                          \
    "vpshufd $78, %%zmm8, %%zmm14\n\t"                                                    \
    "vpshufd $78, %%zmm5, %%zmm7\n\t"                                                     \
    "vpxorq %%zmm8, %%zmm14, %%zmm14\n\t" /* zmm14 holds 5|…|8:a0+a1 */                   \
    "vpxorq %%zmm5, %%zmm7, %%zmm7\n\t"   /* zmm7 holds 5|…|8:b0+b1 */                    \
    "vpclmulqdq $0, %%zmm5, %%zmm8, %%zmm6\n\t"  /* zmm6 holds 5|…|8:a0*b0 */             \
    "vpclmulqdq $17, %%zmm8, %%zmm5, %%zmm5\n\t" /* zmm5 holds 5|…|8:a1*b1 */             \
    "vpclmulqdq $0, %%zmm14, %%zmm7, %%zmm7\n\t" /* zmm7 holds 5|…|8:(a0+a1)*(b0+b1) */   \
                                                                                          \
    "vpternlogq $0x96, %%zmm6, %%zmm17, %%zmm3\n\t" /* zmm3 holds 5+…|…|…+32:a0*b0 */     \
    "vpternlogq $0x96, %%zmm5, %%zmm18, %%zmm1\n\t" /* zmm1 holds 5+…|…|…+32:a1*b1 */     \
    "vpternlogq $0x96, %%zmm7, %%zmm19, %%zmm4\n\t" /* zmm4 holds 5+…|…|…+32:(a0+a1)*(b0+b1) */\
                                                                                          \
    "vpshufd $78, %%zmm16, %%zmm14\n\t"                                                   \
    "vpshufd $78, %%zmm2, %%zmm7\n\t"                                                     \
    "vpxorq %%zmm16, %%zmm14, %%zmm14\n\t" /* zmm14 holds 1|…|4:a0+a1 */                  \
    "vpxorq %%zmm2, %%zmm7, %%zmm7\n\t"   /* zmm7 holds 1|2:b0+b1 */                      \
    "vpclmulqdq $0, %%zmm2, %%zmm16, %%zmm6\n\t"  /* zmm6 holds 1|2:a0*b0 */              \
    "vpclmulqdq $17, %%zmm16, %%zmm2, %%zmm2\n\t" /* zmm2 holds 1|2:a1*b1 */              \
    "vpclmulqdq $0, %%zmm14, %%zmm7, %%zmm7\n\t" /* zmm7 holds 1|2:(a0+a1)*(b0+b1) */     \
                                                                                          \
    "vpxorq %%zmm6, %%zmm3, %%zmm3\n\t" /* zmm3 holds 1+3+…+15|2+4+…+16:a0*b0 */          \
    "vpxorq %%zmm2, %%zmm1, %%zmm1\n\t" /* zmm1 holds 1+3+…+15|2+4+…+16:a1*b1 */          \
    "vpxorq %%zmm7, %%zmm4, %%zmm4\n\t" /* zmm4 holds 1+3+…+15|2+4+…+16:(a0+a1)*(b0+b1) */\
                                                                                          \
    /* aggregated reduction... */                                                         \
    "vpternlogq $0x96, %%zmm1, %%zmm3, %%zmm4\n\t" /* zmm4 holds                          \
                                                    * a0*b0+a1*b1+(a0+a1)*(b0+b1) */      \
    "vpslldq $8, %%zmm4, %%zmm5\n\t"                                                      \
    "vpsrldq $8, %%zmm4, %%zmm4\n\t"                                                      \
    "vpxorq %%zmm5, %%zmm3, %%zmm3\n\t"                                                   \
    "vpxorq %%zmm4, %%zmm1, %%zmm1\n\t" /* <zmm1:zmm3> holds the result of the            \
                                          carry-less multiplication of zmm0               \
                                          by zmm1 */                                      \
                                                                                          \
    /* first phase of the reduction */                                                    \
    "vpsllq $1, %%zmm3, %%zmm6\n\t"  /* packed right shifting << 63 */                    \
    "vpxorq %%zmm3, %%zmm6, %%zmm6\n\t"                                                   \
    "vpsllq $57, %%zmm3, %%zmm5\n\t"  /* packed right shifting << 57 */                   \
    "vpsllq $62, %%zmm6, %%zmm6\n\t"  /* packed right shifting << 62 */                   \
    "vpxorq %%zmm5, %%zmm6, %%zmm6\n\t" /* xor the shifted versions */                    \
    "vpshufd $0x6a, %%zmm6, %%zmm5\n\t"                                                   \
    "vpshufd $0xae, %%zmm6, %%zmm6\n\t"                                                   \
    "vpxorq %%zmm5, %%zmm3, %%zmm3\n\t" /* first phase of the reduction complete */       \
                                                                                          \
    /* second phase of the reduction */                                                   \
    "vpsrlq $1, %%zmm3, %%zmm2\n\t"    /* packed left shifting >> 1 */                    \
    "vpsrlq $2, %%zmm3, %%zmm4\n\t"    /* packed left shifting >> 2 */                    \
    "vpsrlq $7, %%zmm3, %%zmm5\n\t"    /* packed left shifting >> 7 */                    \
    "vpternlogq $0x96, %%zmm3, %%zmm2, %%zmm1\n\t" /* xor the shifted versions */         \
    "vpternlogq $0x96, %%zmm4, %%zmm5, %%zmm6\n\t"                                        \
    "vpxorq %%zmm6, %%zmm1, %%zmm1\n\t" /* the result is in zmm1 */                       \
                                                                                          \
    /* merge 256-bit halves */                                                            \
    "vextracti64x4 $1, %%zmm1, %%ymm2\n\t"                                                \
    "vpxor %%ymm2, %%ymm1, %%ymm1\n\t"                                                    \
    /* merge 128-bit halves */                                                            \
    "vextracti128 $1, %%ymm1, %%xmm2\n\t"                                                 \
    "vpxor %%xmm2, %%xmm1, %%xmm1\n\t"

static ASM_FUNC_ATTR_INLINE void
gfmul_vpclmul_avx512_aggr32(const void *buf, const void *h_table)
{
  /* Input:
      Hx: ZMM0, ZMM8, ZMM9, ZMM10, ZMM11, ZMM12, ZMM13, ZMM16
      bemask: ZMM15
      Hash: XMM1
    Output:
      Hash: XMM1
    Inputs ZMM0, ZMM8, ZMM9, ZMM10, ZMM11, ZMM12, ZMM13, ZMM16 and YMM15 stay
    unmodified.
  */
  asm volatile (GFMUL_AGGR32_ASM_VPCMUL_AVX512(be_to_le)
		:
		: [buf] "r" (buf),
		  [h_table] "r" (h_table)
		: "memory" );
}

static ASM_FUNC_ATTR_INLINE void
gfmul_vpclmul_avx512_aggr32_le(const void *buf, const void *h_table)
{
  /* Input:
      Hx: ZMM0, ZMM8, ZMM9, ZMM10, ZMM11, ZMM12, ZMM13, ZMM16
      bemask: ZMM15
      Hash: XMM1
    Output:
      Hash: XMM1
    Inputs ZMM0, ZMM8, ZMM9, ZMM10, ZMM11, ZMM12, ZMM13, ZMM16 and YMM15 stay
    unmodified.
  */
  asm volatile (GFMUL_AGGR32_ASM_VPCMUL_AVX512(le_to_le)
		:
		: [buf] "r" (buf),
		  [h_table] "r" (h_table)
		: "memory" );
}

static ASM_FUNC_ATTR_INLINE
void gfmul_pclmul_avx512(void)
{
  /* Input: ZMM0 and ZMM1, Output: ZMM1. Input ZMM0 stays unmodified.
     Input must be converted to little-endian.
   */
  asm volatile (/* gfmul, zmm0 has operator a and zmm1 has operator b. */
		"vpshufd $78, %%zmm0, %%zmm2\n\t"
		"vpshufd $78, %%zmm1, %%zmm4\n\t"
		"vpxorq %%zmm0, %%zmm2, %%zmm2\n\t" /* zmm2 holds a0+a1 */
		"vpxorq %%zmm1, %%zmm4, %%zmm4\n\t" /* zmm4 holds b0+b1 */

		"vpclmulqdq $0, %%zmm1, %%zmm0, %%zmm3\n\t"  /* zmm3 holds a0*b0 */
		"vpclmulqdq $17, %%zmm0, %%zmm1, %%zmm1\n\t" /* zmm6 holds a1*b1 */
		"vpclmulqdq $0, %%zmm2, %%zmm4, %%zmm4\n\t"  /* zmm4 holds (a0+a1)*(b0+b1) */

		"vpternlogq $0x96, %%zmm1, %%zmm3, %%zmm4\n\t" /* zmm4 holds
								* a0*b0+a1*b1+(a0+a1)*(b0+b1) */
		"vpslldq $8, %%zmm4, %%zmm5\n\t"
		"vpsrldq $8, %%zmm4, %%zmm4\n\t"
		"vpxorq %%zmm5, %%zmm3, %%zmm3\n\t"
		"vpxorq %%zmm4, %%zmm1, %%zmm1\n\t" /* <zmm1:zmm3> holds the result of the
						      carry-less multiplication of zmm0
						      by zmm1 */

		/* first phase of the reduction */
		"vpsllq $1, %%zmm3, %%zmm6\n\t"  /* packed right shifting << 63 */
		"vpxorq %%zmm3, %%zmm6, %%zmm6\n\t"
		"vpsllq $57, %%zmm3, %%zmm5\n\t"  /* packed right shifting << 57 */
		"vpsllq $62, %%zmm6, %%zmm6\n\t"  /* packed right shifting << 62 */
		"vpxorq %%zmm5, %%zmm6, %%zmm6\n\t" /* xor the shifted versions */
		"vpshufd $0x6a, %%zmm6, %%zmm5\n\t"
		"vpshufd $0xae, %%zmm6, %%zmm6\n\t"
		"vpxorq %%zmm5, %%zmm3, %%zmm3\n\t" /* first phase of the reduction complete */

		/* second phase of the reduction */
		"vpsrlq $1, %%zmm3, %%zmm2\n\t"    /* packed left shifting >> 1 */
		"vpsrlq $2, %%zmm3, %%zmm4\n\t"    /* packed left shifting >> 2 */
		"vpsrlq $7, %%zmm3, %%zmm5\n\t"    /* packed left shifting >> 7 */
		"vpternlogq $0x96, %%zmm3, %%zmm2, %%zmm1\n\t" /* xor the shifted versions */
		"vpternlogq $0x96, %%zmm4, %%zmm5, %%zmm6\n\t"
		"vpxorq %%zmm6, %%zmm1, %%zmm1\n\t" /* the result is in zmm1 */
                ::: "memory" );
}

static ASM_FUNC_ATTR_INLINE void
gcm_lsh_avx512(void *h, unsigned int hoffs)
{
  static const u64 pconst[8] __attribute__ ((aligned (64))) =
    {
      U64_C(0x0000000000000001), U64_C(0xc200000000000000),
      U64_C(0x0000000000000001), U64_C(0xc200000000000000),
      U64_C(0x0000000000000001), U64_C(0xc200000000000000),
      U64_C(0x0000000000000001), U64_C(0xc200000000000000)
    };

  asm volatile ("vmovdqu64 %[h], %%zmm2\n\t"
                "vpshufd $0xff, %%zmm2, %%zmm3\n\t"
                "vpsrad $31, %%zmm3, %%zmm3\n\t"
                "vpslldq $8, %%zmm2, %%zmm4\n\t"
                "vpandq %[pconst], %%zmm3, %%zmm3\n\t"
                "vpaddq %%zmm2, %%zmm2, %%zmm2\n\t"
                "vpsrlq $63, %%zmm4, %%zmm4\n\t"
                "vpternlogq $0x96, %%zmm4, %%zmm3, %%zmm2\n\t"
                "vmovdqu64 %%zmm2, %[h]\n\t"
                : [h] "+m" (*((byte *)h + hoffs))
                : [pconst] "m" (*pconst)
                : "memory" );
}

static ASM_FUNC_ATTR_INLINE void
load_h1h4_to_zmm1(gcry_cipher_hd_t c)
{
  unsigned int key_pos =
    offsetof(struct gcry_cipher_handle, u_mode.gcm.u_ghash_key.key);
  unsigned int table_pos =
    offsetof(struct gcry_cipher_handle, u_mode.gcm.gcm_table);

  if (key_pos + 16 == table_pos)
    {
      /* Optimization: Table follows immediately after key. */
      asm volatile ("vmovdqu64 %[key], %%zmm1\n\t"
		    :
		    : [key] "m" (*c->u_mode.gcm.u_ghash_key.key)
		    : "memory");
    }
  else
    {
      asm volatile ("vmovdqu64 -1*16(%[h_table]), %%zmm1\n\t"
		    "vinserti64x2 $0, %[key], %%zmm1, %%zmm1\n\t"
		    :
		    : [h_table] "r" (c->u_mode.gcm.gcm_table),
		      [key] "m" (*c->u_mode.gcm.u_ghash_key.key)
		    : "memory");
    }
}

static ASM_FUNC_ATTR void
ghash_setup_aggr8_avx512(gcry_cipher_hd_t c)
{
  c->u_mode.gcm.hw_impl_flags |= GCM_INTEL_AGGR8_TABLE_INITIALIZED;

  asm volatile (/* load H⁴ */
		"vbroadcasti64x2 3*16(%[h_table]), %%zmm0\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");
  /* load H <<< 1, H² <<< 1, H³ <<< 1, H⁴ <<< 1 */
  load_h1h4_to_zmm1 (c);

  gfmul_pclmul_avx512 (); /* H<<<1•H⁴ => H⁵, …, H⁴<<<1•H⁴ => H⁸ */

  asm volatile ("vmovdqu64 %%zmm1, 4*16(%[h_table])\n\t" /* store H⁸ for aggr16 setup */
		"vmovdqu64 %%zmm1, 3*16(%[h_table])\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gcm_lsh_avx512 (c->u_mode.gcm.gcm_table, 3 * 16); /* H⁵ <<< 1, …, H⁸ <<< 1 */
}

static ASM_FUNC_ATTR void
ghash_setup_aggr16_avx512(gcry_cipher_hd_t c)
{
  c->u_mode.gcm.hw_impl_flags |= GCM_INTEL_AGGR16_TABLE_INITIALIZED;

  asm volatile (/* load H⁸ */
		"vbroadcasti64x2 7*16(%[h_table]), %%zmm0\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");
  /* load H <<< 1, H² <<< 1, H³ <<< 1, H⁴ <<< 1 */
  load_h1h4_to_zmm1 (c);

  gfmul_pclmul_avx512 (); /* H<<<1•H⁸ => H⁹, … , H⁴<<<1•H⁸ => H¹² */

  asm volatile ("vmovdqu64 %%zmm1, 7*16(%[h_table])\n\t"
		/* load H⁵ <<< 1, …, H⁸ <<< 1 */
		"vmovdqu64 3*16(%[h_table]), %%zmm1\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul_avx512 (); /* H⁵<<<1•H⁸ => H¹¹, … , H⁸<<<1•H⁸ => H¹⁶ */

  asm volatile ("vmovdqu64 %%zmm1, 12*16(%[h_table])\n\t" /* store H¹⁶ for aggr32 setup */
                "vmovdqu64 %%zmm1, 11*16(%[h_table])\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gcm_lsh_avx512 (c->u_mode.gcm.gcm_table, 7 * 16); /* H⁹ <<< 1, …, H¹² <<< 1 */
  gcm_lsh_avx512 (c->u_mode.gcm.gcm_table, 11 * 16); /* H¹³ <<< 1, …, H¹⁶ <<< 1 */
}

static ASM_FUNC_ATTR void
ghash_setup_aggr32_avx512(gcry_cipher_hd_t c)
{
  c->u_mode.gcm.hw_impl_flags |= GCM_INTEL_AGGR32_TABLE_INITIALIZED;

  asm volatile (/* load H¹⁶ */
		"vbroadcasti64x2 15*16(%[h_table]), %%zmm0\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");
  /* load H <<< 1, H² <<< 1, H³ <<< 1, H⁴ <<< 1 */
  load_h1h4_to_zmm1 (c);

  gfmul_pclmul_avx512 (); /* H<<<1•H¹⁶ => H¹⁷, …, H⁴<<<1•H¹⁶ => H²⁰ */

  asm volatile ("vmovdqu64 %%zmm1, 15*16(%[h_table])\n\t"
		/* load H⁵ <<< 1, …, H⁸ <<< 1 */
		"vmovdqu64 3*16(%[h_table]), %%zmm1\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul_avx512 (); /* H⁵<<<1•H¹⁶ => H²¹, …, H⁹<<<1•H¹⁶ => H²⁴ */

  asm volatile ("vmovdqu64 %%zmm1, 19*16(%[h_table])\n\t"
		/* load H⁹ <<< 1, …, H¹² <<< 1 */
		"vmovdqu64 7*16(%[h_table]), %%zmm1\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul_avx512 (); /* H⁹<<<1•H¹⁶ => H²⁵, …, H¹²<<<1•H¹⁶ => H²⁸ */

  asm volatile ("vmovdqu64 %%zmm1, 23*16(%[h_table])\n\t"
		/* load H¹³ <<< 1, …, H¹⁶ <<< 1 */
		"vmovdqu64 11*16(%[h_table]), %%zmm1\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul_avx512 (); /* H¹³<<<1•H¹⁶ => H²⁹, …, H¹⁶<<<1•H¹⁶ => H³² */

  asm volatile ("vmovdqu64 %%zmm1, 27*16(%[h_table])\n\t"
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gcm_lsh_avx512 (c->u_mode.gcm.gcm_table, 15 * 16);
  gcm_lsh_avx512 (c->u_mode.gcm.gcm_table, 19 * 16);
  gcm_lsh_avx512 (c->u_mode.gcm.gcm_table, 23 * 16);
  gcm_lsh_avx512 (c->u_mode.gcm.gcm_table, 27 * 16);
}

static const u64 swap128b_perm[8] __attribute__ ((aligned (64))) =
  {
    /* For swapping order of 128bit lanes in 512bit register using vpermq. */
    6, 7, 4, 5, 2, 3, 0, 1
  };

#endif /* GCM_USE_INTEL_VPCLMUL_AVX512 */
#endif /* __x86_64__ */

static unsigned int ASM_FUNC_ATTR
_gcry_ghash_intel_pclmul (gcry_cipher_hd_t c, byte *result, const byte *buf,
			  size_t nblocks);

static unsigned int ASM_FUNC_ATTR
_gcry_polyval_intel_pclmul (gcry_cipher_hd_t c, byte *result, const byte *buf,
			    size_t nblocks);

static ASM_FUNC_ATTR_INLINE void
gcm_lsh(void *h, unsigned int hoffs)
{
  static const u64 pconst[2] __attribute__ ((aligned (16))) =
    { U64_C(0x0000000000000001), U64_C(0xc200000000000000) };

  asm volatile ("movdqu %[h], %%xmm2\n\t"
                "pshufd $0xff, %%xmm2, %%xmm3\n\t"
                "movdqa %%xmm2, %%xmm4\n\t"
                "psrad $31, %%xmm3\n\t"
                "pslldq $8, %%xmm4\n\t"
                "pand %[pconst], %%xmm3\n\t"
                "paddq %%xmm2, %%xmm2\n\t"
                "psrlq $63, %%xmm4\n\t"
                "pxor %%xmm3, %%xmm2\n\t"
                "pxor %%xmm4, %%xmm2\n\t"
                "movdqu %%xmm2, %[h]\n\t"
                : [h] "+m" (*((byte *)h + hoffs))
                : [pconst] "m" (*pconst)
                : "memory" );
}

void ASM_FUNC_ATTR
_gcry_ghash_setup_intel_pclmul (gcry_cipher_hd_t c, unsigned int hw_features)
{
  static const unsigned char be_mask[16] __attribute__ ((aligned (16))) =
    { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
#if defined(__x86_64__) && defined(__WIN64__)
  char win64tmp[10 * 16];

  /* XMM6-XMM15 need to be restored after use. */
  asm volatile ("movdqu %%xmm6,  0*16(%0)\n\t"
                "movdqu %%xmm7,  1*16(%0)\n\t"
                "movdqu %%xmm8,  2*16(%0)\n\t"
                "movdqu %%xmm9,  3*16(%0)\n\t"
                "movdqu %%xmm10, 4*16(%0)\n\t"
                "movdqu %%xmm11, 5*16(%0)\n\t"
                "movdqu %%xmm12, 6*16(%0)\n\t"
                "movdqu %%xmm13, 7*16(%0)\n\t"
                "movdqu %%xmm14, 8*16(%0)\n\t"
                "movdqu %%xmm15, 9*16(%0)\n\t"
                :
                : "r" (win64tmp)
                : "memory" );
#endif

  (void)hw_features;

  c->u_mode.gcm.hw_impl_flags = 0;
  c->u_mode.gcm.ghash_fn = _gcry_ghash_intel_pclmul;
  c->u_mode.gcm.polyval_fn = _gcry_polyval_intel_pclmul;

  /* Swap endianness of hsub. */
  asm volatile ("movdqu (%[key]), %%xmm0\n\t"
                "pshufb %[be_mask], %%xmm0\n\t"
                "movdqu %%xmm0, (%[key])\n\t"
                :
                : [key] "r" (c->u_mode.gcm.u_ghash_key.key),
                  [be_mask] "m" (*be_mask)
                : "memory");

  gcm_lsh (c->u_mode.gcm.u_ghash_key.key, 0); /* H <<< 1 */

  asm volatile ("movdqa %%xmm0, %%xmm1\n\t"
                "movdqu (%[key]), %%xmm0\n\t" /* load H <<< 1 */
                :
                : [key] "r" (c->u_mode.gcm.u_ghash_key.key)
                : "memory");

  gfmul_pclmul (); /* H<<<1•H => H² */

  asm volatile ("movdqu %%xmm1, 0*16(%[h_table])\n\t"
                :
                : [h_table] "r" (c->u_mode.gcm.gcm_table)
                : "memory");

  gcm_lsh (c->u_mode.gcm.gcm_table, 0 * 16); /* H² <<< 1 */

  if (0)
    { }
#ifdef GCM_USE_INTEL_VPCLMUL_AVX2
  else if ((hw_features & HWF_INTEL_VAES_VPCLMUL)
           && (hw_features & HWF_INTEL_AVX2))
    {
      c->u_mode.gcm.hw_impl_flags |= GCM_INTEL_USE_VPCLMUL_AVX2;

#ifdef GCM_USE_INTEL_VPCLMUL_AVX512
      if (hw_features & HWF_INTEL_AVX512)
	c->u_mode.gcm.hw_impl_flags |= GCM_INTEL_USE_VPCLMUL_AVX512;
#endif

      asm volatile (/* H² */
		    "vinserti128 $1, %%xmm1, %%ymm1, %%ymm1\n\t"
		    /* load H <<< 1, H² <<< 1 */
		    "vinserti128 $1, 0*16(%[h_table]), %%ymm0, %%ymm0\n\t"
		    :
		    : [h_table] "r" (c->u_mode.gcm.gcm_table)
		    : "memory");

      gfmul_pclmul_avx2 (); /* H<<<1•H² => H³, H²<<<1•H² => H⁴ */

      asm volatile ("vmovdqu %%ymm1, 2*16(%[h_table])\n\t" /* store H⁴ for aggr8 setup */
		    "vmovdqu %%ymm1, 1*16(%[h_table])\n\t"
		    :
		    : [h_table] "r" (c->u_mode.gcm.gcm_table)
		    : "memory");

      gcm_lsh_avx2 (c->u_mode.gcm.gcm_table, 1 * 16); /* H³ <<< 1, H⁴ <<< 1 */

      asm volatile ("vzeroupper\n\t"
		    ::: "memory" );
    }
#endif /* GCM_USE_INTEL_VPCLMUL_AVX2 */
  else
    {
      asm volatile ("movdqa %%xmm1, %%xmm7\n\t"
		    ::: "memory");

      gfmul_pclmul (); /* H<<<1•H² => H³ */

      asm volatile ("movdqa %%xmm7, %%xmm0\n\t"
		    "movdqu %%xmm1, 1*16(%[h_table])\n\t"
		    "movdqu 0*16(%[h_table]), %%xmm1\n\t" /* load H² <<< 1 */
		    :
		    : [h_table] "r" (c->u_mode.gcm.gcm_table)
		    : "memory");

      gfmul_pclmul (); /* H²<<<1•H² => H⁴ */

      asm volatile ("movdqu %%xmm1, 3*16(%[h_table])\n\t" /* store H⁴ for aggr8 setup */
		    "movdqu %%xmm1, 2*16(%[h_table])\n\t"
		    :
		    : [h_table] "r" (c->u_mode.gcm.gcm_table)
		    : "memory");

      gcm_lsh (c->u_mode.gcm.gcm_table, 1 * 16); /* H³ <<< 1 */
      gcm_lsh (c->u_mode.gcm.gcm_table, 2 * 16); /* H⁴ <<< 1 */
    }

  /* Clear/restore used registers. */
  asm volatile ("pxor %%xmm0, %%xmm0\n\t"
		"pxor %%xmm1, %%xmm1\n\t"
		"pxor %%xmm2, %%xmm2\n\t"
		"pxor %%xmm3, %%xmm3\n\t"
		"pxor %%xmm4, %%xmm4\n\t"
		"pxor %%xmm5, %%xmm5\n\t"
		"pxor %%xmm6, %%xmm6\n\t"
		"pxor %%xmm7, %%xmm7\n\t"
		::: "memory" );
#ifdef __x86_64__
#ifdef __WIN64__
  asm volatile ("movdqu 0*16(%0), %%xmm6\n\t"
                "movdqu 1*16(%0), %%xmm7\n\t"
                "movdqu 2*16(%0), %%xmm8\n\t"
                "movdqu 3*16(%0), %%xmm9\n\t"
                "movdqu 4*16(%0), %%xmm10\n\t"
                "movdqu 5*16(%0), %%xmm11\n\t"
                "movdqu 6*16(%0), %%xmm12\n\t"
                "movdqu 7*16(%0), %%xmm13\n\t"
                "movdqu 8*16(%0), %%xmm14\n\t"
                "movdqu 9*16(%0), %%xmm15\n\t"
                :
                : "r" (win64tmp)
                : "memory" );
#else
  asm volatile ("pxor %%xmm8, %%xmm8\n\t"
                "pxor %%xmm9, %%xmm9\n\t"
                "pxor %%xmm10, %%xmm10\n\t"
                "pxor %%xmm11, %%xmm11\n\t"
                "pxor %%xmm12, %%xmm12\n\t"
                "pxor %%xmm13, %%xmm13\n\t"
                "pxor %%xmm14, %%xmm14\n\t"
                "pxor %%xmm15, %%xmm15\n\t"
                ::: "memory" );
#endif /* __WIN64__ */
#endif /* __x86_64__ */
}


#ifdef __x86_64__
static ASM_FUNC_ATTR void
ghash_setup_aggr8(gcry_cipher_hd_t c)
{
  c->u_mode.gcm.hw_impl_flags |= GCM_INTEL_AGGR8_TABLE_INITIALIZED;

  asm volatile ("movdqa 3*16(%[h_table]), %%xmm0\n\t" /* load H⁴ */
		"movdqu %[key], %%xmm1\n\t" /* load H <<< 1 */
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table),
		  [key] "m" (*c->u_mode.gcm.u_ghash_key.key)
		: "memory");

  gfmul_pclmul (); /* H<<<1•H⁴ => H⁵ */

  asm volatile ("movdqu %%xmm1, 3*16(%[h_table])\n\t"
		"movdqu 0*16(%[h_table]), %%xmm1\n\t" /* load H² <<< 1 */
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul (); /* H²<<<1•H⁴ => H⁶ */

  asm volatile ("movdqu %%xmm1, 4*16(%[h_table])\n\t"
		"movdqu 1*16(%[h_table]), %%xmm1\n\t" /* load H³ <<< 1 */
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul (); /* H³<<<1•H⁴ => H⁷ */

  asm volatile ("movdqu %%xmm1, 5*16(%[h_table])\n\t"
		"movdqu 2*16(%[h_table]), %%xmm1\n\t" /* load H⁴ <<< 1 */
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gfmul_pclmul (); /* H⁴<<<1•H⁴ => H⁸ */

  asm volatile ("movdqu %%xmm1, 6*16(%[h_table])\n\t"
		"movdqu %%xmm1, 7*16(%[h_table])\n\t" /* store H⁸ for aggr16 setup */
		:
		: [h_table] "r" (c->u_mode.gcm.gcm_table)
		: "memory");

  gcm_lsh (c->u_mode.gcm.gcm_table, 3 * 16); /* H⁵ <<< 1 */
  gcm_lsh (c->u_mode.gcm.gcm_table, 4 * 16); /* H⁶ <<< 1 */
  gcm_lsh (c->u_mode.gcm.gcm_table, 5 * 16); /* H⁷ <<< 1 */
  gcm_lsh (c->u_mode.gcm.gcm_table, 6 * 16); /* H⁸ <<< 1 */
}
#endif /* __x86_64__ */


unsigned int ASM_FUNC_ATTR
_gcry_ghash_intel_pclmul (gcry_cipher_hd_t c, byte *result, const byte *buf,
			  size_t nblocks)
{
  static const unsigned char be_mask[16] __attribute__ ((aligned (16))) =
    { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
  const unsigned int blocksize = GCRY_GCM_BLOCK_LEN;
#if defined(__x86_64__) && defined(__WIN64__)
  char win64tmp[10 * 16];
#endif

  if (nblocks == 0)
    return 0;

#if defined(__x86_64__) && defined(__WIN64__)
  /* XMM6-XMM15 need to be restored after use. */
  asm volatile ("movdqu %%xmm6,  0*16(%0)\n\t"
                "movdqu %%xmm7,  1*16(%0)\n\t"
                "movdqu %%xmm8,  2*16(%0)\n\t"
                "movdqu %%xmm9,  3*16(%0)\n\t"
                "movdqu %%xmm10, 4*16(%0)\n\t"
                "movdqu %%xmm11, 5*16(%0)\n\t"
                "movdqu %%xmm12, 6*16(%0)\n\t"
                "movdqu %%xmm13, 7*16(%0)\n\t"
                "movdqu %%xmm14, 8*16(%0)\n\t"
                "movdqu %%xmm15, 9*16(%0)\n\t"
                :
                : "r" (win64tmp)
                : "memory" );
#endif

  /* Preload hash. */
  asm volatile ("movdqa %[be_mask], %%xmm7\n\t"
                "movdqu %[hash], %%xmm1\n\t"
                "pshufb %%xmm7, %%xmm1\n\t" /* be => le */
                :
                : [hash] "m" (*result),
                  [be_mask] "m" (*be_mask)
                : "memory" );

#if defined(GCM_USE_INTEL_VPCLMUL_AVX2)
  if (nblocks >= 16
      && ((c->u_mode.gcm.hw_impl_flags & GCM_INTEL_USE_VPCLMUL_AVX2)
          || (c->u_mode.gcm.hw_impl_flags & GCM_INTEL_USE_VPCLMUL_AVX512)))
    {
#if defined(GCM_USE_INTEL_VPCLMUL_AVX512)
      if (nblocks >= 32
	  && (c->u_mode.gcm.hw_impl_flags & GCM_INTEL_USE_VPCLMUL_AVX512))
	{
	  asm volatile ("vpopcntb %%xmm7, %%xmm16\n\t" /* spec stop for old AVX512 CPUs */
			"vshufi64x2 $0, %%zmm7, %%zmm7, %%zmm15\n\t"
			"vmovdqa %%xmm1, %%xmm8\n\t"
			"vmovdqu64 %[swapperm], %%zmm14\n\t"
			:
			: [swapperm] "m" (swap128b_perm),
			  [h_table] "r" (c->u_mode.gcm.gcm_table)
			: "memory" );

	  if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR32_TABLE_INITIALIZED))
	    {
	      if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR16_TABLE_INITIALIZED))
		{
		  if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR8_TABLE_INITIALIZED))
		    ghash_setup_aggr8_avx512 (c); /* Clobbers registers XMM0-XMM7. */

		  ghash_setup_aggr16_avx512 (c); /* Clobbers registers XMM0-XMM7. */
		}

	      ghash_setup_aggr32_avx512 (c); /* Clobbers registers XMM0-XMM7. */
	    }

	  /* Preload H1-H32. */
	  load_h1h4_to_zmm1 (c);
	  asm volatile ("vpermq %%zmm1, %%zmm14, %%zmm16\n\t" /* H1|H2|H3|H4 */
			"vmovdqa %%xmm8, %%xmm1\n\t"
			"vpermq 27*16(%[h_table]), %%zmm14, %%zmm0\n\t"  /* H28|H29|H31|H32 */
			"vpermq 23*16(%[h_table]), %%zmm14, %%zmm13\n\t" /* H25|H26|H27|H28 */
			"vpermq 19*16(%[h_table]), %%zmm14, %%zmm12\n\t" /* H21|H22|H23|H24 */
			"vpermq 15*16(%[h_table]), %%zmm14, %%zmm11\n\t" /* H17|H18|H19|H20 */
			"vpermq 11*16(%[h_table]), %%zmm14, %%zmm10\n\t" /* H13|H14|H15|H16 */
			"vpermq 7*16(%[h_table]), %%zmm14, %%zmm9\n\t"   /* H9|H10|H11|H12 */
			"vpermq 3*16(%[h_table]), %%zmm14, %%zmm8\n\t"   /* H4|H6|H7|H8 */
			:
			: [h_1] "m" (*c->u_mode.gcm.u_ghash_key.key),
			  [h_table] "r" (c->u_mode.gcm.gcm_table)
			: "memory" );

	  while (nblocks >= 32)
	    {
	      gfmul_vpclmul_avx512_aggr32 (buf, c->u_mode.gcm.gcm_table);

	      buf += 32 * blocksize;
	      nblocks -= 32;
	    }

	  asm volatile ("vmovdqa %%xmm15, %%xmm7\n\t"
			"vpxorq %%ymm16, %%ymm16, %%ymm16\n\t"
			"vpxorq %%ymm17, %%ymm17, %%ymm17\n\t"
			"vpxorq %%ymm18, %%ymm18, %%ymm18\n\t"
			"vpxorq %%ymm19, %%ymm19, %%ymm19\n\t"
			:
			:
			: "memory" );
	}
#endif /* GCM_USE_INTEL_VPCLMUL_AVX512 */

      if (nblocks >= 16)
	{
	  u64 h1_h2_h15_h16[4*2];

	  asm volatile ("vinserti128 $1, %%xmm7, %%ymm7, %%ymm15\n\t"
			"vmovdqa %%xmm1, %%xmm8\n\t"
			::: "memory" );

	  if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR16_TABLE_INITIALIZED))
	    {
	      if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR8_TABLE_INITIALIZED))
		ghash_setup_aggr8_avx2 (c); /* Clobbers registers XMM0-XMM7. */

	      ghash_setup_aggr16_avx2 (c); /* Clobbers registers XMM0-XMM7. */
	    }

	  /* Preload H1-H16. */
	  load_h1h2_to_ymm1 (c);
	  asm volatile ("vperm2i128 $0x23, %%ymm1, %%ymm1, %%ymm7\n\t" /* H1|H2 */
			"vmovdqa %%xmm8, %%xmm1\n\t"
			"vpxor %%xmm8, %%xmm8, %%xmm8\n\t"
			"vperm2i128 $0x23, 13*16(%[h_table]), %%ymm8, %%ymm0\n\t"  /* H15|H16 */
			"vperm2i128 $0x23, 11*16(%[h_table]), %%ymm8, %%ymm13\n\t" /* H13|H14 */
			"vperm2i128 $0x23, 9*16(%[h_table]), %%ymm8, %%ymm12\n\t"  /* H11|H12 */
			"vperm2i128 $0x23, 7*16(%[h_table]), %%ymm8, %%ymm11\n\t"  /* H9|H10 */
			"vperm2i128 $0x23, 5*16(%[h_table]), %%ymm8, %%ymm10\n\t"  /* H7|H8 */
			"vperm2i128 $0x23, 3*16(%[h_table]), %%ymm8, %%ymm9\n\t"   /* H5|H6 */
			"vperm2i128 $0x23, 1*16(%[h_table]), %%ymm8, %%ymm8\n\t"   /* H3|H4 */
			"vmovdqu %%ymm0, %[h15_h16]\n\t"
			"vmovdqu %%ymm7, %[h1_h2]\n\t"
			: [h1_h2] "=m" (h1_h2_h15_h16[0]),
			  [h15_h16] "=m" (h1_h2_h15_h16[4])
			: [h_1] "m" (*c->u_mode.gcm.u_ghash_key.key),
			  [h_table] "r" (c->u_mode.gcm.gcm_table)
			: "memory" );

	  while (nblocks >= 16)
	    {
	      gfmul_vpclmul_avx2_aggr16 (buf, c->u_mode.gcm.gcm_table,
					h1_h2_h15_h16);

	      buf += 16 * blocksize;
	      nblocks -= 16;
	    }

	  asm volatile ("vmovdqu %%ymm15, %[h15_h16]\n\t"
			"vmovdqu %%ymm15, %[h1_h2]\n\t"
			"vmovdqa %%xmm15, %%xmm7\n\t"
			:
			  [h1_h2] "=m" (h1_h2_h15_h16[0]),
			  [h15_h16] "=m" (h1_h2_h15_h16[4])
			:
			: "memory" );
	}

      asm volatile ("vzeroupper\n\t" ::: "memory" );
    }
#endif /* GCM_USE_INTEL_VPCLMUL_AVX2 */

#ifdef __x86_64__
  if (nblocks >= 8)
    {
      asm volatile ("movdqa %%xmm7, %%xmm15\n\t"
		    "movdqa %%xmm1, %%xmm8\n\t"
		    ::: "memory" );

      if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR8_TABLE_INITIALIZED))
	ghash_setup_aggr8 (c); /* Clobbers registers XMM0-XMM7. */

      /* Preload H1. */
      asm volatile ("movdqa %%xmm8, %%xmm1\n\t"
		    "movdqa %[h_1], %%xmm0\n\t"
		    :
		    : [h_1] "m" (*c->u_mode.gcm.u_ghash_key.key)
		    : "memory" );

      while (nblocks >= 8)
        {
          gfmul_pclmul_aggr8 (buf, c->u_mode.gcm.gcm_table);

          buf += 8 * blocksize;
          nblocks -= 8;
        }
    }
#endif /* __x86_64__ */

  while (nblocks >= 4)
    {
      gfmul_pclmul_aggr4 (buf, c->u_mode.gcm.u_ghash_key.key,
                          c->u_mode.gcm.gcm_table, be_mask);

      buf += 4 * blocksize;
      nblocks -= 4;
    }

  if (nblocks)
    {
      /* Preload H1. */
      asm volatile ("movdqa %[h_1], %%xmm0\n\t"
                    :
                    : [h_1] "m" (*c->u_mode.gcm.u_ghash_key.key)
                    : "memory" );

      while (nblocks)
        {
          asm volatile ("movdqu %[buf], %%xmm2\n\t"
                        "pshufb %[be_mask], %%xmm2\n\t" /* be => le */
                        "pxor %%xmm2, %%xmm1\n\t"
                        :
                        : [buf] "m" (*buf), [be_mask] "m" (*be_mask)
                        : "memory" );

          gfmul_pclmul ();

          buf += blocksize;
          nblocks--;
        }
    }

  /* Store hash. */
  asm volatile ("pshufb %[be_mask], %%xmm1\n\t" /* be => le */
                "movdqu %%xmm1, %[hash]\n\t"
                : [hash] "=m" (*result)
                : [be_mask] "m" (*be_mask)
                : "memory" );

  /* Clear/restore used registers. */
  asm volatile ("pxor %%xmm0, %%xmm0\n\t"
		"pxor %%xmm1, %%xmm1\n\t"
		"pxor %%xmm2, %%xmm2\n\t"
		"pxor %%xmm3, %%xmm3\n\t"
		"pxor %%xmm4, %%xmm4\n\t"
		"pxor %%xmm5, %%xmm5\n\t"
		"pxor %%xmm6, %%xmm6\n\t"
		"pxor %%xmm7, %%xmm7\n\t"
		:
		:
		: "memory" );
#ifdef __x86_64__
#ifdef __WIN64__
  asm volatile ("movdqu 0*16(%0), %%xmm6\n\t"
		"movdqu 1*16(%0), %%xmm7\n\t"
		"movdqu 2*16(%0), %%xmm8\n\t"
		"movdqu 3*16(%0), %%xmm9\n\t"
		"movdqu 4*16(%0), %%xmm10\n\t"
		"movdqu 5*16(%0), %%xmm11\n\t"
		"movdqu 6*16(%0), %%xmm12\n\t"
		"movdqu 7*16(%0), %%xmm13\n\t"
		"movdqu 8*16(%0), %%xmm14\n\t"
		"movdqu 9*16(%0), %%xmm15\n\t"
		:
		: "r" (win64tmp)
		: "memory" );
#else
  /* Clear used registers. */
  asm volatile (
		"pxor %%xmm8, %%xmm8\n\t"
		"pxor %%xmm9, %%xmm9\n\t"
		"pxor %%xmm10, %%xmm10\n\t"
		"pxor %%xmm11, %%xmm11\n\t"
		"pxor %%xmm12, %%xmm12\n\t"
		"pxor %%xmm13, %%xmm13\n\t"
		"pxor %%xmm14, %%xmm14\n\t"
		"pxor %%xmm15, %%xmm15\n\t"
		:
		:
		: "memory" );
#endif /* __WIN64__ */
#endif /* __x86_64__ */

  return 0;
}

unsigned int ASM_FUNC_ATTR
_gcry_polyval_intel_pclmul (gcry_cipher_hd_t c, byte *result, const byte *buf,
			    size_t nblocks)
{
  static const unsigned char be_mask[16] __attribute__ ((aligned (16))) =
    { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
  const unsigned int blocksize = GCRY_GCM_BLOCK_LEN;
#if defined(__x86_64__) && defined(__WIN64__)
  char win64tmp[10 * 16];
#endif

  if (nblocks == 0)
    return 0;

#if defined(__x86_64__) && defined(__WIN64__)
  /* XMM6-XMM15 need to be restored after use. */
  asm volatile ("movdqu %%xmm6,  0*16(%0)\n\t"
                "movdqu %%xmm7,  1*16(%0)\n\t"
                "movdqu %%xmm8,  2*16(%0)\n\t"
                "movdqu %%xmm9,  3*16(%0)\n\t"
                "movdqu %%xmm10, 4*16(%0)\n\t"
                "movdqu %%xmm11, 5*16(%0)\n\t"
                "movdqu %%xmm12, 6*16(%0)\n\t"
                "movdqu %%xmm13, 7*16(%0)\n\t"
                "movdqu %%xmm14, 8*16(%0)\n\t"
                "movdqu %%xmm15, 9*16(%0)\n\t"
                :
                : "r" (win64tmp)
                : "memory" );
#endif

  /* Preload hash. */
  asm volatile ("pxor %%xmm7, %%xmm7\n\t"
                "movdqu %[hash], %%xmm1\n\t"
                "pshufb %[be_mask], %%xmm1\n\t" /* be => le */
                :
                : [hash] "m" (*result),
                  [be_mask] "m" (*be_mask)
                : "memory" );

#if defined(GCM_USE_INTEL_VPCLMUL_AVX2)
  if (nblocks >= 16
      && ((c->u_mode.gcm.hw_impl_flags & GCM_INTEL_USE_VPCLMUL_AVX2)
          || (c->u_mode.gcm.hw_impl_flags & GCM_INTEL_USE_VPCLMUL_AVX512)))
    {
#if defined(GCM_USE_INTEL_VPCLMUL_AVX512)
      if (nblocks >= 32
	  && (c->u_mode.gcm.hw_impl_flags & GCM_INTEL_USE_VPCLMUL_AVX512))
	{
	  asm volatile ("vpopcntb %%xmm1, %%xmm16\n\t" /* spec stop for old AVX512 CPUs */
			"vmovdqa %%xmm1, %%xmm8\n\t"
			"vmovdqu64 %[swapperm], %%zmm14\n\t"
			:
			: [swapperm] "m" (swap128b_perm),
			  [h_table] "r" (c->u_mode.gcm.gcm_table)
			: "memory" );

	  if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR32_TABLE_INITIALIZED))
	    {
	      if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR16_TABLE_INITIALIZED))
		{
		  if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR8_TABLE_INITIALIZED))
		    ghash_setup_aggr8_avx512 (c); /* Clobbers registers XMM0-XMM7. */

		  ghash_setup_aggr16_avx512 (c); /* Clobbers registers XMM0-XMM7. */
		}

	      ghash_setup_aggr32_avx512 (c); /* Clobbers registers XMM0-XMM7. */
	    }

	  /* Preload H1-H32. */
	  load_h1h4_to_zmm1 (c);
	  asm volatile ("vpermq %%zmm1, %%zmm14, %%zmm16\n\t" /* H1|H2|H3|H4 */
			"vmovdqa %%xmm8, %%xmm1\n\t"
			"vpermq 27*16(%[h_table]), %%zmm14, %%zmm0\n\t"  /* H28|H29|H31|H32 */
			"vpermq 23*16(%[h_table]), %%zmm14, %%zmm13\n\t" /* H25|H26|H27|H28 */
			"vpermq 19*16(%[h_table]), %%zmm14, %%zmm12\n\t" /* H21|H22|H23|H24 */
			"vpermq 15*16(%[h_table]), %%zmm14, %%zmm11\n\t" /* H17|H18|H19|H20 */
			"vpermq 11*16(%[h_table]), %%zmm14, %%zmm10\n\t" /* H13|H14|H15|H16 */
			"vpermq 7*16(%[h_table]), %%zmm14, %%zmm9\n\t"   /* H9|H10|H11|H12 */
			"vpermq 3*16(%[h_table]), %%zmm14, %%zmm8\n\t"   /* H4|H6|H7|H8 */
			:
			: [h_1] "m" (*c->u_mode.gcm.u_ghash_key.key),
			  [h_table] "r" (c->u_mode.gcm.gcm_table)
			: "memory" );

	  while (nblocks >= 32)
	    {
	      gfmul_vpclmul_avx512_aggr32_le (buf, c->u_mode.gcm.gcm_table);

	      buf += 32 * blocksize;
	      nblocks -= 32;
	    }

	  asm volatile ("vpxor %%xmm7, %%xmm7, %%xmm7\n\t"
			"vpxorq %%ymm16, %%ymm16, %%ymm16\n\t"
			"vpxorq %%ymm17, %%ymm17, %%ymm17\n\t"
			"vpxorq %%ymm18, %%ymm18, %%ymm18\n\t"
			"vpxorq %%ymm19, %%ymm19, %%ymm19\n\t"
			:
			:
			: "memory" );
	}
#endif /* GCM_USE_INTEL_VPCLMUL_AVX512 */

      if (nblocks >= 16)
	{
	  u64 h1_h2_h15_h16[4*2];

	  asm volatile ("vmovdqa %%xmm1, %%xmm8\n\t"
			::: "memory" );

	  if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR16_TABLE_INITIALIZED))
	    {
	      if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR8_TABLE_INITIALIZED))
		ghash_setup_aggr8_avx2 (c); /* Clobbers registers XMM0-XMM7. */

	      ghash_setup_aggr16_avx2 (c); /* Clobbers registers XMM0-XMM7. */
	    }

	  /* Preload H1-H16. */
	  load_h1h2_to_ymm1 (c);
	  asm volatile ("vperm2i128 $0x23, %%ymm1, %%ymm1, %%ymm7\n\t" /* H1|H2 */
			"vmovdqa %%xmm8, %%xmm1\n\t"
			"vpxor %%xmm8, %%xmm8, %%xmm8\n\t"
			"vperm2i128 $0x23, 13*16(%[h_table]), %%ymm8, %%ymm0\n\t"  /* H15|H16 */
			"vperm2i128 $0x23, 11*16(%[h_table]), %%ymm8, %%ymm13\n\t" /* H13|H14 */
			"vperm2i128 $0x23, 9*16(%[h_table]), %%ymm8, %%ymm12\n\t"  /* H11|H12 */
			"vperm2i128 $0x23, 7*16(%[h_table]), %%ymm8, %%ymm11\n\t"  /* H9|H10 */
			"vperm2i128 $0x23, 5*16(%[h_table]), %%ymm8, %%ymm10\n\t"  /* H7|H8 */
			"vperm2i128 $0x23, 3*16(%[h_table]), %%ymm8, %%ymm9\n\t"   /* H5|H6 */
			"vperm2i128 $0x23, 1*16(%[h_table]), %%ymm8, %%ymm8\n\t"   /* H3|H4 */
			"vmovdqu %%ymm0, %[h15_h16]\n\t"
			"vmovdqu %%ymm7, %[h1_h2]\n\t"
			: [h1_h2] "=m" (h1_h2_h15_h16[0]),
			  [h15_h16] "=m" (h1_h2_h15_h16[4])
			: [h_1] "m" (*c->u_mode.gcm.u_ghash_key.key),
			  [h_table] "r" (c->u_mode.gcm.gcm_table)
			: "memory" );

	  while (nblocks >= 16)
	    {
	      gfmul_vpclmul_avx2_aggr16_le (buf, c->u_mode.gcm.gcm_table,
					    h1_h2_h15_h16);

	      buf += 16 * blocksize;
	      nblocks -= 16;
	    }

	  asm volatile ("vpxor %%xmm7, %%xmm7, %%xmm7\n\t"
			"vmovdqu %%ymm7, %[h15_h16]\n\t"
			"vmovdqu %%ymm7, %[h1_h2]\n\t"
			: [h1_h2] "=m" (h1_h2_h15_h16[0]),
			  [h15_h16] "=m" (h1_h2_h15_h16[4])
			:
			: "memory" );
	}

      asm volatile ("vzeroupper\n\t" ::: "memory" );
    }
#endif /* GCM_USE_INTEL_VPCLMUL_AVX2 */

#ifdef __x86_64__
  if (nblocks >= 8)
    {
      asm volatile ("movdqa %%xmm1, %%xmm8\n\t"
		    ::: "memory" );

      if (!(c->u_mode.gcm.hw_impl_flags & GCM_INTEL_AGGR8_TABLE_INITIALIZED))
	ghash_setup_aggr8 (c); /* Clobbers registers XMM0-XMM7. */

      /* Preload H1. */
      asm volatile ("movdqa %%xmm8, %%xmm1\n\t"
		    "pxor %%xmm15, %%xmm15\n\t"
		    "movdqa %[h_1], %%xmm0\n\t"
		    :
		    : [h_1] "m" (*c->u_mode.gcm.u_ghash_key.key)
		    : "memory" );

      while (nblocks >= 8)
        {
          gfmul_pclmul_aggr8_le (buf, c->u_mode.gcm.gcm_table);

          buf += 8 * blocksize;
          nblocks -= 8;
        }
    }
#endif

  while (nblocks >= 4)
    {
      gfmul_pclmul_aggr4_le (buf, c->u_mode.gcm.u_ghash_key.key,
                             c->u_mode.gcm.gcm_table);

      buf += 4 * blocksize;
      nblocks -= 4;
    }

  if (nblocks)
    {
      /* Preload H1. */
      asm volatile ("movdqa %[h_1], %%xmm0\n\t"
                    :
                    : [h_1] "m" (*c->u_mode.gcm.u_ghash_key.key)
                    : "memory" );

      while (nblocks)
        {
          asm volatile ("movdqu %[buf], %%xmm2\n\t"
                        "pxor %%xmm2, %%xmm1\n\t"
                        :
                        : [buf] "m" (*buf)
                        : "memory" );

          gfmul_pclmul ();

          buf += blocksize;
          nblocks--;
        }
    }

  /* Store hash. */
  asm volatile ("pshufb %[be_mask], %%xmm1\n\t" /* be => le */
                "movdqu %%xmm1, %[hash]\n\t"
                : [hash] "=m" (*result)
                : [be_mask] "m" (*be_mask)
                : "memory" );

  /* Clear/restore used registers. */
  asm volatile ("pxor %%xmm0, %%xmm0\n\t"
		"pxor %%xmm1, %%xmm1\n\t"
		"pxor %%xmm2, %%xmm2\n\t"
		"pxor %%xmm3, %%xmm3\n\t"
		"pxor %%xmm4, %%xmm4\n\t"
		"pxor %%xmm5, %%xmm5\n\t"
		"pxor %%xmm6, %%xmm6\n\t"
		"pxor %%xmm7, %%xmm7\n\t"
		:
		:
		: "memory" );
#ifdef __x86_64__
#ifdef __WIN64__
  asm volatile ("movdqu 0*16(%0), %%xmm6\n\t"
		"movdqu 1*16(%0), %%xmm7\n\t"
		"movdqu 2*16(%0), %%xmm8\n\t"
		"movdqu 3*16(%0), %%xmm9\n\t"
		"movdqu 4*16(%0), %%xmm10\n\t"
		"movdqu 5*16(%0), %%xmm11\n\t"
		"movdqu 6*16(%0), %%xmm12\n\t"
		"movdqu 7*16(%0), %%xmm13\n\t"
		"movdqu 8*16(%0), %%xmm14\n\t"
		"movdqu 9*16(%0), %%xmm15\n\t"
		:
		: "r" (win64tmp)
		: "memory" );
#else
  /* Clear used registers. */
  asm volatile (
		"pxor %%xmm8, %%xmm8\n\t"
		"pxor %%xmm9, %%xmm9\n\t"
		"pxor %%xmm10, %%xmm10\n\t"
		"pxor %%xmm11, %%xmm11\n\t"
		"pxor %%xmm12, %%xmm12\n\t"
		"pxor %%xmm13, %%xmm13\n\t"
		"pxor %%xmm14, %%xmm14\n\t"
		"pxor %%xmm15, %%xmm15\n\t"
		:
		:
		: "memory" );
#endif /* __WIN64__ */
#endif /* __x86_64__ */

  return 0;
}

#if __clang__
#  pragma clang attribute pop
#endif

#endif /* GCM_USE_INTEL_PCLMUL */
