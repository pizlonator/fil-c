/* sm3-armv8-aarch64-ce.S  -  ARMv8/AArch64/CE accelerated SM3 cipher
 *
 * Copyright (C) 2022 Alibaba Group.
 * Copyright (C) 2022 Tianjia Zhang <tianjia.zhang@linux.alibaba.com>
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

#include "asm-common-aarch64.h"

#if defined(__AARCH64EL__) && \
    defined(HAVE_COMPATIBLE_GCC_AARCH64_PLATFORM_AS) && \
    defined(HAVE_GCC_INLINE_ASM_AARCH64_CRYPTO) && \
    defined(USE_SM3)

.cpu generic+simd+crypto

/* Must be consistent with register macros */
#define vecnum_v0       0
#define vecnum_v1       1
#define vecnum_v2       2
#define vecnum_v3       3
#define vecnum_v4       4
#define vecnum_CTX1     16
#define vecnum_CTX2     17
#define vecnum_SS1      18
#define vecnum_WT       19
#define vecnum_K0       20
#define vecnum_K1       21
#define vecnum_K2       22
#define vecnum_K3       23
#define vecnum_RTMP0    24
#define vecnum_RTMP1    25

#define sm3partw1(vd, vn, vm) \
    .inst (0xce60c000 | (vecnum_##vm << 16) | (vecnum_##vn << 5) | vecnum_##vd)

#define sm3partw2(vd, vn, vm) \
    .inst (0xce60c400 | (vecnum_##vm << 16) | (vecnum_##vn << 5) | vecnum_##vd)

#define sm3ss1(vd, vn, vm, va) \
    .inst (0xce400000 | (vecnum_##vm << 16) | (vecnum_##va << 10) \
            | (vecnum_##vn << 5) | vecnum_##vd)

#define sm3tt1a(vd, vn, vm, imm2) \
    .inst (0xce408000 | (vecnum_##vm << 16) | imm2 << 12 \
            | (vecnum_##vn << 5) | vecnum_##vd)

#define sm3tt1b(vd, vn, vm, imm2) \
    .inst (0xce408400 | (vecnum_##vm << 16) | imm2 << 12 \
            | (vecnum_##vn << 5) | vecnum_##vd)

#define sm3tt2a(vd, vn, vm, imm2) \
    .inst (0xce408800 | (vecnum_##vm << 16) | imm2 << 12 \
            | (vecnum_##vn << 5) | vecnum_##vd)

#define sm3tt2b(vd, vn, vm, imm2) \
    .inst (0xce408c00 | (vecnum_##vm << 16) | imm2 << 12 \
            | (vecnum_##vn << 5) | vecnum_##vd)

/* Constants */

SECTION_RODATA
.align 4
ELF(.type _gcry_sm3_armv8_ce_consts,@object)
_gcry_sm3_armv8_ce_consts:
.Lsm3_Ktable:
    .long 0x79cc4519, 0xf3988a32, 0xe7311465, 0xce6228cb
    .long 0x9cc45197, 0x3988a32f, 0x7311465e, 0xe6228cbc
    .long 0xcc451979, 0x988a32f3, 0x311465e7, 0x6228cbce
    .long 0xc451979c, 0x88a32f39, 0x11465e73, 0x228cbce6
    .long 0x9d8a7a87, 0x3b14f50f, 0x7629ea1e, 0xec53d43c
    .long 0xd8a7a879, 0xb14f50f3, 0x629ea1e7, 0xc53d43ce
    .long 0x8a7a879d, 0x14f50f3b, 0x29ea1e76, 0x53d43cec
    .long 0xa7a879d8, 0x4f50f3b1, 0x9ea1e762, 0x3d43cec5
    .long 0x7a879d8a, 0xf50f3b14, 0xea1e7629, 0xd43cec53
    .long 0xa879d8a7, 0x50f3b14f, 0xa1e7629e, 0x43cec53d
    .long 0x879d8a7a, 0x0f3b14f5, 0x1e7629ea, 0x3cec53d4
    .long 0x79d8a7a8, 0xf3b14f50, 0xe7629ea1, 0xcec53d43
    .long 0x9d8a7a87, 0x3b14f50f, 0x7629ea1e, 0xec53d43c
    .long 0xd8a7a879, 0xb14f50f3, 0x629ea1e7, 0xc53d43ce
    .long 0x8a7a879d, 0x14f50f3b, 0x29ea1e76, 0x53d43cec
    .long 0xa7a879d8, 0x4f50f3b1, 0x9ea1e762, 0x3d43cec5
ELF(.size _gcry_sm3_armv8_ce_consts,.-_gcry_sm3_armv8_ce_consts)

/* Register macros */

/* Must be consistent with vecnum_ macros */
#define CTX1    v16
#define CTX2    v17
#define SS1     v18
#define WT      v19

#define K0      v20
#define K1      v21
#define K2      v22
#define K3      v23

#define RTMP0   v24
#define RTMP1   v25

/* Helper macros. */

#define _(...) /*_*/

#define SCHED_W_1(s0, s1, s2, s3, s4) ext       s4.16b, s1.16b, s2.16b, #12
#define SCHED_W_2(s0, s1, s2, s3, s4) ext       RTMP0.16b, s0.16b, s1.16b, #12
#define SCHED_W_3(s0, s1, s2, s3, s4) ext       RTMP1.16b, s2.16b, s3.16b, #8
#define SCHED_W_4(s0, s1, s2, s3, s4) sm3partw1(s4, s0, s3)
#define SCHED_W_5(s0, s1, s2, s3, s4) sm3partw2(s4, RTMP1, RTMP0)

#define SCHED_W(n, s0, s1, s2, s3, s4) SCHED_W_##n(s0, s1, s2, s3, s4)

#define R(ab, s0, s1, s2, s3, s4, IOP)                  \
        ld4     {K0.s, K1.s, K2.s, K3.s}[3], [x3], #16; \
        eor     WT.16b, s0.16b, s1.16b;                 \
                                                        \
        sm3ss1(SS1, CTX1, CTX2, K0);                    \
      IOP(1, s0, s1, s2, s3, s4);                       \
        sm3tt1##ab(CTX1, SS1, WT, 0);                   \
        sm3tt2##ab(CTX2, SS1, s0, 0);                   \
                                                        \
      IOP(2, s0, s1, s2, s3, s4);                       \
        sm3ss1(SS1, CTX1, CTX2, K1);                    \
      IOP(3, s0, s1, s2, s3, s4);                       \
        sm3tt1##ab(CTX1, SS1, WT, 1);                   \
        sm3tt2##ab(CTX2, SS1, s0, 1);                   \
                                                        \
        sm3ss1(SS1, CTX1, CTX2, K2);                    \
      IOP(4, s0, s1, s2, s3, s4);                       \
        sm3tt1##ab(CTX1, SS1, WT, 2);                   \
        sm3tt2##ab(CTX2, SS1, s0, 2);                   \
                                                        \
        sm3ss1(SS1, CTX1, CTX2, K3);                    \
      IOP(5, s0, s1, s2, s3, s4);                       \
        sm3tt1##ab(CTX1, SS1, WT, 3);                   \
        sm3tt2##ab(CTX2, SS1, s0, 3);

#define R1(s0, s1, s2, s3, s4, IOP)  R(a, s0, s1, s2, s3, s4, IOP)
#define R2(s0, s1, s2, s3, s4, IOP)  R(b, s0, s1, s2, s3, s4, IOP)


.text

.align 4
.global _gcry_sm3_transform_armv8_ce
ELF(.type _gcry_sm3_transform_armv8_ce,%function;)
_gcry_sm3_transform_armv8_ce:
    /* input:
     *   x0: CTX
     *   x1: data
     *   x2: nblocks
     */
    CFI_STARTPROC();

    ld1         {CTX1.4s, CTX2.4s}, [x0];
    rev64       CTX1.4s, CTX1.4s;
    rev64       CTX2.4s, CTX2.4s;
    ext         CTX1.16b, CTX1.16b, CTX1.16b, #8;
    ext         CTX2.16b, CTX2.16b, CTX2.16b, #8;

.Lloop:
    GET_DATA_POINTER(x3, .Lsm3_Ktable);
    ld1         {v0.16b-v3.16b}, [x1], #64;
    sub         x2, x2, #1;

    mov         v6.16b, CTX1.16b;
    mov         v7.16b, CTX2.16b;

    rev32       v0.16b, v0.16b;
    rev32       v1.16b, v1.16b;
    rev32       v2.16b, v2.16b;
    rev32       v3.16b, v3.16b;

    R1(v0, v1, v2, v3, v4, SCHED_W);
    R1(v1, v2, v3, v4, v0, SCHED_W);
    R1(v2, v3, v4, v0, v1, SCHED_W);
    R1(v3, v4, v0, v1, v2, SCHED_W);
    R2(v4, v0, v1, v2, v3, SCHED_W);
    R2(v0, v1, v2, v3, v4, SCHED_W);
    R2(v1, v2, v3, v4, v0, SCHED_W);
    R2(v2, v3, v4, v0, v1, SCHED_W);
    R2(v3, v4, v0, v1, v2, SCHED_W);
    R2(v4, v0, v1, v2, v3, SCHED_W);
    R2(v0, v1, v2, v3, v4, SCHED_W);
    R2(v1, v2, v3, v4, v0, SCHED_W);
    R2(v2, v3, v4, v0, v1, SCHED_W);
    R2(v3, v4, v0, v1, v2, _);
    R2(v4, v0, v1, v2, v3, _);
    R2(v0, v1, v2, v3, v4, _);

    eor         CTX1.16b, CTX1.16b, v6.16b;
    eor         CTX2.16b, CTX2.16b, v7.16b;

    cbnz        x2, .Lloop;

    /* save state */
    rev64       CTX1.4s, CTX1.4s;
    rev64       CTX2.4s, CTX2.4s;
    ext         CTX1.16b, CTX1.16b, CTX1.16b, #8;
    ext         CTX2.16b, CTX2.16b, CTX2.16b, #8;
    st1         {CTX1.4s, CTX2.4s}, [x0];

    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm3_transform_armv8_ce, .-_gcry_sm3_transform_armv8_ce;)

#endif
