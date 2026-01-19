/* sm4-armv9-aarch64-sve-ce.S - ARMv9/AArch64 SVE Cryptography accelerated SM4
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
    defined(HAVE_GCC_INLINE_ASM_AARCH64_SVE) && \
    defined(HAVE_GCC_INLINE_ASM_AARCH64_SVE2) && \
    defined(USE_SM4)

.cpu generic+simd+crypto+sve+sve2

/* Constants */

SECTION_RODATA
.align 4
ELF(.type _gcry_sm4_armv9_svesm4_consts,@object)
_gcry_sm4_armv9_svesm4_consts:
.Lbswap128_mask:
    .byte 0x0c, 0x0d, 0x0e, 0x0f, 0x08, 0x09, 0x0a, 0x0b
    .byte 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03
    .byte 0x1c, 0x1d, 0x1e, 0x1f, 0x18, 0x19, 0x1a, 0x1b
    .byte 0x14, 0x15, 0x16, 0x17, 0x10, 0x11, 0x12, 0x13
    .byte 0x2c, 0x2d, 0x2e, 0x2f, 0x28, 0x29, 0x2a, 0x2b
    .byte 0x24, 0x25, 0x26, 0x27, 0x20, 0x21, 0x22, 0x23
    .byte 0x3c, 0x3d, 0x3e, 0x3f, 0x38, 0x39, 0x3a, 0x3b
    .byte 0x34, 0x35, 0x36, 0x37, 0x30, 0x31, 0x32, 0x33
    .byte 0x4c, 0x4d, 0x4e, 0x4f, 0x48, 0x49, 0x4a, 0x4b
    .byte 0x44, 0x45, 0x46, 0x47, 0x40, 0x41, 0x42, 0x43
    .byte 0x5c, 0x5d, 0x5e, 0x5f, 0x58, 0x59, 0x5a, 0x5b
    .byte 0x54, 0x55, 0x56, 0x57, 0x50, 0x51, 0x52, 0x53
    .byte 0x6c, 0x6d, 0x6e, 0x6f, 0x68, 0x69, 0x6a, 0x6b
    .byte 0x64, 0x65, 0x66, 0x67, 0x60, 0x61, 0x62, 0x63
    .byte 0x7c, 0x7d, 0x7e, 0x7f, 0x78, 0x79, 0x7a, 0x7b
    .byte 0x74, 0x75, 0x76, 0x77, 0x70, 0x71, 0x72, 0x73
    .byte 0x8c, 0x8d, 0x8e, 0x8f, 0x88, 0x89, 0x8a, 0x8b
    .byte 0x84, 0x85, 0x86, 0x87, 0x80, 0x81, 0x82, 0x83
    .byte 0x9c, 0x9d, 0x9e, 0x9f, 0x98, 0x99, 0x9a, 0x9b
    .byte 0x94, 0x95, 0x96, 0x97, 0x90, 0x91, 0x92, 0x93
    .byte 0xac, 0xad, 0xae, 0xaf, 0xa8, 0xa9, 0xaa, 0xab
    .byte 0xa4, 0xa5, 0xa6, 0xa7, 0xa0, 0xa1, 0xa2, 0xa3
    .byte 0xbc, 0xbd, 0xbe, 0xbf, 0xb8, 0xb9, 0xba, 0xbb
    .byte 0xb4, 0xb5, 0xb6, 0xb7, 0xb0, 0xb1, 0xb2, 0xb3
    .byte 0xcc, 0xcd, 0xce, 0xcf, 0xc8, 0xc9, 0xca, 0xcb
    .byte 0xc4, 0xc5, 0xc6, 0xc7, 0xc0, 0xc1, 0xc2, 0xc3
    .byte 0xdc, 0xdd, 0xde, 0xdf, 0xd8, 0xd9, 0xda, 0xdb
    .byte 0xd4, 0xd5, 0xd6, 0xd7, 0xd0, 0xd1, 0xd2, 0xd3
    .byte 0xec, 0xed, 0xee, 0xef, 0xe8, 0xe9, 0xea, 0xeb
    .byte 0xe4, 0xe5, 0xe6, 0xe7, 0xe0, 0xe1, 0xe2, 0xe3
    .byte 0xfc, 0xfd, 0xfe, 0xff, 0xf8, 0xf9, 0xfa, 0xfb
    .byte 0xf4, 0xf5, 0xf6, 0xf7, 0xf0, 0xf1, 0xf2, 0xf3

.Lle128_inc:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
ELF(.size _gcry_sm4_armv9_svesm4_consts,.-_gcry_sm4_armv9_svesm4_consts)

/* Register macros */

#define RCTR        z16
#define RCTRv       v16
#define RIV         z16
#define RIVv        v16
#define RSWAP128    z17
#define RZERO       z18
#define RLE128_INC  z19

#define RTMP0       z20
#define RTMP1       z21
#define RTMP2       z22
#define RTMP3       z23
#define RTMP0v      v20

#define vecnum_z0   0
#define vecnum_z1   1
#define vecnum_z2   2
#define vecnum_z3   3
#define vecnum_z4   4
#define vecnum_z5   5
#define vecnum_z6   6
#define vecnum_z7   7
#define vecnum_z8   8
#define vecnum_z9   9
#define vecnum_z10  10
#define vecnum_z11  11
#define vecnum_z12  12
#define vecnum_z13  13
#define vecnum_z14  14
#define vecnum_z15  15
#define vecnum_z16  16
#define vecnum_z24  24
#define vecnum_z25  25
#define vecnum_z26  26
#define vecnum_z27  27
#define vecnum_z28  28
#define vecnum_z29  29
#define vecnum_z30  30
#define vecnum_z31  31

#define vecnum_v0 0
#define vecnum_v15 15
#define vecnum_v24 24
#define vecnum_v25 25
#define vecnum_v26 26
#define vecnum_v27 27
#define vecnum_v28 28
#define vecnum_v29 29
#define vecnum_v30 30
#define vecnum_v31 31

#define sm4e_ce(vd, vn) \
    .inst (0xcec08400 | (vecnum_##vn << 5) | vecnum_##vd)

#define sm4e_sve(zd, zm) \
    .inst (0x4523e000 | (vecnum_##zm << 5) | vecnum_##zd)

/* Helper macros. */

#define PREPARE()                                   \
        GET_DATA_POINTER(x7, .Lbswap128_mask);     \
        ptrue       p0.b, ALL;                      \
        rdvl        x5, #1;                         \
        ld1b        {RSWAP128.b}, p0/z, [x7];       \
                                                    \
        ld1         {v24.16b-v27.16b}, [x0], #64;   \
        ld1         {v28.16b-v31.16b}, [x0];        \
        dup         z24.q, z24.q[0];                \
        dup         z25.q, z25.q[0];                \
        dup         z26.q, z26.q[0];                \
        dup         z27.q, z27.q[0];                \
        dup         z28.q, z28.q[0];                \
        dup         z29.q, z29.q[0];                \
        dup         z30.q, z30.q[0];                \
        dup         z31.q, z31.q[0];


#define SM4_SVE_CE_CRYPT_BLK(b0)                    \
        revb        b0.s, p0/m, b0.s;               \
        sm4e_sve(b0, z24);                          \
        sm4e_sve(b0, z25);                          \
        sm4e_sve(b0, z26);                          \
        sm4e_sve(b0, z27);                          \
        sm4e_sve(b0, z28);                          \
        sm4e_sve(b0, z29);                          \
        sm4e_sve(b0, z30);                          \
        sm4e_sve(b0, z31);                          \
        tbl         b0.b, {b0.b}, RSWAP128.b;       \
        revb        b0.s, p0/m, b0.s;


#define SM4_SVE_CE_CRYPT_BLK4(b0, b1, b2, b3)       \
        revb        b0.s, p0/m, b0.s;               \
        revb        b1.s, p0/m, b1.s;               \
        revb        b2.s, p0/m, b2.s;               \
        revb        b3.s, p0/m, b3.s;               \
        sm4e_sve(b0, z24);                          \
        sm4e_sve(b1, z24);                          \
        sm4e_sve(b2, z24);                          \
        sm4e_sve(b3, z24);                          \
        sm4e_sve(b0, z25);                          \
        sm4e_sve(b1, z25);                          \
        sm4e_sve(b2, z25);                          \
        sm4e_sve(b3, z25);                          \
        sm4e_sve(b0, z26);                          \
        sm4e_sve(b1, z26);                          \
        sm4e_sve(b2, z26);                          \
        sm4e_sve(b3, z26);                          \
        sm4e_sve(b0, z27);                          \
        sm4e_sve(b1, z27);                          \
        sm4e_sve(b2, z27);                          \
        sm4e_sve(b3, z27);                          \
        sm4e_sve(b0, z28);                          \
        sm4e_sve(b1, z28);                          \
        sm4e_sve(b2, z28);                          \
        sm4e_sve(b3, z28);                          \
        sm4e_sve(b0, z29);                          \
        sm4e_sve(b1, z29);                          \
        sm4e_sve(b2, z29);                          \
        sm4e_sve(b3, z29);                          \
        sm4e_sve(b0, z30);                          \
        sm4e_sve(b1, z30);                          \
        sm4e_sve(b2, z30);                          \
        sm4e_sve(b3, z30);                          \
        sm4e_sve(b0, z31);                          \
        sm4e_sve(b1, z31);                          \
        sm4e_sve(b2, z31);                          \
        sm4e_sve(b3, z31);                          \
        tbl         b0.b, {b0.b}, RSWAP128.b;       \
        tbl         b1.b, {b1.b}, RSWAP128.b;       \
        tbl         b2.b, {b2.b}, RSWAP128.b;       \
        tbl         b3.b, {b3.b}, RSWAP128.b;       \
        revb        b0.s, p0/m, b0.s;               \
        revb        b1.s, p0/m, b1.s;               \
        revb        b2.s, p0/m, b2.s;               \
        revb        b3.s, p0/m, b3.s;


#define SM4_SVE_CE_CRYPT_BLK8(b0, b1, b2, b3, b4, b5, b6, b7) \
        revb        b0.s, p0/m, b0.s;               \
        revb        b1.s, p0/m, b1.s;               \
        revb        b2.s, p0/m, b2.s;               \
        revb        b3.s, p0/m, b3.s;               \
        revb        b4.s, p0/m, b4.s;               \
        revb        b5.s, p0/m, b5.s;               \
        revb        b6.s, p0/m, b6.s;               \
        revb        b7.s, p0/m, b7.s;               \
        sm4e_sve(b0, z24);                          \
        sm4e_sve(b1, z24);                          \
        sm4e_sve(b2, z24);                          \
        sm4e_sve(b3, z24);                          \
        sm4e_sve(b4, z24);                          \
        sm4e_sve(b5, z24);                          \
        sm4e_sve(b6, z24);                          \
        sm4e_sve(b7, z24);                          \
        sm4e_sve(b0, z25);                          \
        sm4e_sve(b1, z25);                          \
        sm4e_sve(b2, z25);                          \
        sm4e_sve(b3, z25);                          \
        sm4e_sve(b4, z25);                          \
        sm4e_sve(b5, z25);                          \
        sm4e_sve(b6, z25);                          \
        sm4e_sve(b7, z25);                          \
        sm4e_sve(b0, z26);                          \
        sm4e_sve(b1, z26);                          \
        sm4e_sve(b2, z26);                          \
        sm4e_sve(b3, z26);                          \
        sm4e_sve(b4, z26);                          \
        sm4e_sve(b5, z26);                          \
        sm4e_sve(b6, z26);                          \
        sm4e_sve(b7, z26);                          \
        sm4e_sve(b0, z27);                          \
        sm4e_sve(b1, z27);                          \
        sm4e_sve(b2, z27);                          \
        sm4e_sve(b3, z27);                          \
        sm4e_sve(b4, z27);                          \
        sm4e_sve(b5, z27);                          \
        sm4e_sve(b6, z27);                          \
        sm4e_sve(b7, z27);                          \
        sm4e_sve(b0, z28);                          \
        sm4e_sve(b1, z28);                          \
        sm4e_sve(b2, z28);                          \
        sm4e_sve(b3, z28);                          \
        sm4e_sve(b4, z28);                          \
        sm4e_sve(b5, z28);                          \
        sm4e_sve(b6, z28);                          \
        sm4e_sve(b7, z28);                          \
        sm4e_sve(b0, z29);                          \
        sm4e_sve(b1, z29);                          \
        sm4e_sve(b2, z29);                          \
        sm4e_sve(b3, z29);                          \
        sm4e_sve(b4, z29);                          \
        sm4e_sve(b5, z29);                          \
        sm4e_sve(b6, z29);                          \
        sm4e_sve(b7, z29);                          \
        sm4e_sve(b0, z30);                          \
        sm4e_sve(b1, z30);                          \
        sm4e_sve(b2, z30);                          \
        sm4e_sve(b3, z30);                          \
        sm4e_sve(b4, z30);                          \
        sm4e_sve(b5, z30);                          \
        sm4e_sve(b6, z30);                          \
        sm4e_sve(b7, z30);                          \
        sm4e_sve(b0, z31);                          \
        sm4e_sve(b1, z31);                          \
        sm4e_sve(b2, z31);                          \
        sm4e_sve(b3, z31);                          \
        sm4e_sve(b4, z31);                          \
        sm4e_sve(b5, z31);                          \
        sm4e_sve(b6, z31);                          \
        sm4e_sve(b7, z31);                          \
        tbl         b0.b, {b0.b}, RSWAP128.b;       \
        tbl         b1.b, {b1.b}, RSWAP128.b;       \
        tbl         b2.b, {b2.b}, RSWAP128.b;       \
        tbl         b3.b, {b3.b}, RSWAP128.b;       \
        tbl         b4.b, {b4.b}, RSWAP128.b;       \
        tbl         b5.b, {b5.b}, RSWAP128.b;       \
        tbl         b6.b, {b6.b}, RSWAP128.b;       \
        tbl         b7.b, {b7.b}, RSWAP128.b;       \
        revb        b0.s, p0/m, b0.s;               \
        revb        b1.s, p0/m, b1.s;               \
        revb        b2.s, p0/m, b2.s;               \
        revb        b3.s, p0/m, b3.s;               \
        revb        b4.s, p0/m, b4.s;               \
        revb        b5.s, p0/m, b5.s;               \
        revb        b6.s, p0/m, b6.s;               \
        revb        b7.s, p0/m, b7.s;


#define SM4_CE_CRYPT_BLK(b0)                        \
        rev32       b0.16b, b0.16b;                 \
        sm4e_ce(b0, v24);                           \
        sm4e_ce(b0, v25);                           \
        sm4e_ce(b0, v26);                           \
        sm4e_ce(b0, v27);                           \
        sm4e_ce(b0, v28);                           \
        sm4e_ce(b0, v29);                           \
        sm4e_ce(b0, v30);                           \
        sm4e_ce(b0, v31);                           \
        rev64       b0.4s, b0.4s;                   \
        ext         b0.16b, b0.16b, b0.16b, #8;     \
        rev32       b0.16b, b0.16b;


.align 4
.global _gcry_sm4_armv9_sve_ce_crypt
ELF(.type _gcry_sm4_armv9_sve_ce_crypt,%function;)
_gcry_sm4_armv9_sve_ce_crypt:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: nblocks
     */
    CFI_STARTPROC();

    PREPARE();

.Lcrypt_loop_blks:
    sub         x3, x3, x5, LSR #1;         /* x3 - (8 * VL) */
    tbnz        x3, #63, .Lcrypt_tail8;

    ld1b        {z0.b}, p0/z, [x2];
    ld1b        {z1.b}, p0/z, [x2, #1, MUL VL];
    ld1b        {z2.b}, p0/z, [x2, #2, MUL VL];
    ld1b        {z3.b}, p0/z, [x2, #3, MUL VL];
    ld1b        {z4.b}, p0/z, [x2, #4, MUL VL];
    ld1b        {z5.b}, p0/z, [x2, #5, MUL VL];
    ld1b        {z6.b}, p0/z, [x2, #6, MUL VL];
    ld1b        {z7.b}, p0/z, [x2, #7, MUL VL];
    addvl       x2, x2, #8;

    SM4_SVE_CE_CRYPT_BLK8(z0, z1, z2, z3, z4, z5, z6, z7);

    st1b        {z0.b}, p0, [x1];
    st1b        {z1.b}, p0, [x1, #1, MUL VL];
    st1b        {z2.b}, p0, [x1, #2, MUL VL];
    st1b        {z3.b}, p0, [x1, #3, MUL VL];
    st1b        {z4.b}, p0, [x1, #4, MUL VL];
    st1b        {z5.b}, p0, [x1, #5, MUL VL];
    st1b        {z6.b}, p0, [x1, #6, MUL VL];
    st1b        {z7.b}, p0, [x1, #7, MUL VL];
    addvl       x1, x1, #8;

    cbz         x3, .Lcrypt_end;
    b           .Lcrypt_loop_blks;

.Lcrypt_tail8:
    add         x3, x3, x5, LSR #1;
    cmp         x3, x5, LSR #2;
    blt         .Lcrypt_tail4;

    sub         x3, x3, x5, LSR #2;     /* x3 - (4 * VL) */

    ld1b        {z0.b}, p0/z, [x2];
    ld1b        {z1.b}, p0/z, [x2, #1, MUL VL];
    ld1b        {z2.b}, p0/z, [x2, #2, MUL VL];
    ld1b        {z3.b}, p0/z, [x2, #3, MUL VL];
    addvl       x2, x2, #4;

    SM4_SVE_CE_CRYPT_BLK4(z0, z1, z2, z3);

    st1b        {z0.b}, p0, [x1];
    st1b        {z1.b}, p0, [x1, #1, MUL VL];
    st1b        {z2.b}, p0, [x1, #2, MUL VL];
    st1b        {z3.b}, p0, [x1, #3, MUL VL];
    addvl       x1, x1, #4;

    cbz         x3, .Lcrypt_end;

.Lcrypt_tail4:
    cmp         x3, x5, LSR #4;
    blt         .Lcrypt_tail;

    sub         x3, x3, x5, LSR #4;     /* x3 - VL */

    ld1b        {z0.b}, p0/z, [x2];
    addvl       x2, x2, #1;

    SM4_SVE_CE_CRYPT_BLK(z0);

    st1b        {z0.b}, p0, [x1];
    addvl       x1, x1, #1;

    cbz         x3, .Lcrypt_end;

.Lcrypt_tail:
    sub         x3, x3, #1;

    ld1         {v0.16b}, [x2], #16;
    SM4_CE_CRYPT_BLK(v0);
    st1         {v0.16b}, [x1], #16;

    cbnz        x3, .Lcrypt_tail;

.Lcrypt_end:
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv9_sve_ce_crypt,.-_gcry_sm4_armv9_sve_ce_crypt;)

.align 4
.global _gcry_sm4_armv9_sve_ce_cbc_dec
ELF(.type _gcry_sm4_armv9_sve_ce_cbc_dec,%function;)
_gcry_sm4_armv9_sve_ce_cbc_dec:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: iv (big endian, 128 bit)
     *   x4: nblocks
     */
    CFI_STARTPROC();
    VPUSH_ABI;

    PREPARE();
    ld1         {RIVv.16b}, [x3];
    ext         RIV.b, RIV.b, RIV.b, #16;

.Lcbc_loop_blks:
    sub         x4, x4, x5, LSR #1;         /* x4 - (8 * VL) */
    tbnz        x4, #63, .Lcbc_tail8;

    ld1b        {z15.b}, p0/z, [x2];
    ld1b        {z14.b}, p0/z, [x2, #1, MUL VL];
    ld1b        {z13.b}, p0/z, [x2, #2, MUL VL];
    ld1b        {z12.b}, p0/z, [x2, #3, MUL VL];
    ld1b        {z11.b}, p0/z, [x2, #4, MUL VL];
    ld1b        {z10.b}, p0/z, [x2, #5, MUL VL];
    ld1b        {z9.b}, p0/z, [x2, #6, MUL VL];
    ld1b        {z8.b}, p0/z, [x2, #7, MUL VL];
    rev         z0.b, z15.b;
    rev         z1.b, z14.b;
    rev         z2.b, z13.b;
    rev         z3.b, z12.b;
    rev         z4.b, z11.b;
    rev         z5.b, z10.b;
    rev         z6.b, z9.b;
    rev         z7.b, z8.b;
    rev         RTMP0.b, RIV.b;
    ext         z7.b, z7.b, z6.b, #16;
    ext         z6.b, z6.b, z5.b, #16;
    ext         z5.b, z5.b, z4.b, #16;
    ext         z4.b, z4.b, z3.b, #16;
    ext         z3.b, z3.b, z2.b, #16;
    ext         z2.b, z2.b, z1.b, #16;
    ext         z1.b, z1.b, z0.b, #16;
    ext         z0.b, z0.b, RTMP0.b, #16;
    rev         z7.b, z7.b;
    rev         z6.b, z6.b;
    rev         z5.b, z5.b;
    rev         z4.b, z4.b;
    rev         z3.b, z3.b;
    rev         z2.b, z2.b;
    rev         z1.b, z1.b;
    rev         z0.b, z0.b;
    mov         RIV.d, z8.d;

    SM4_SVE_CE_CRYPT_BLK8(z15, z14, z13, z12, z11, z10, z9, z8);

    eor         z0.d, z0.d, z15.d;
    eor         z1.d, z1.d, z14.d;
    eor         z2.d, z2.d, z13.d;
    eor         z3.d, z3.d, z12.d;
    eor         z4.d, z4.d, z11.d;
    eor         z5.d, z5.d, z10.d;
    eor         z6.d, z6.d, z9.d;
    eor         z7.d, z7.d, z8.d;
    st1b        {z0.b}, p0, [x1];
    st1b        {z1.b}, p0, [x1, #1, MUL VL];
    st1b        {z2.b}, p0, [x1, #2, MUL VL];
    st1b        {z3.b}, p0, [x1, #3, MUL VL];
    st1b        {z4.b}, p0, [x1, #4, MUL VL];
    st1b        {z5.b}, p0, [x1, #5, MUL VL];
    st1b        {z6.b}, p0, [x1, #6, MUL VL];
    st1b        {z7.b}, p0, [x1, #7, MUL VL];
    addvl       x2, x2, #8;
    addvl       x1, x1, #8;

    cbz         x4, .Lcbc_end;
    b           .Lcbc_loop_blks;

.Lcbc_tail8:
    add         x4, x4, x5, LSR #1;
    cmp         x4, x5, LSR #2;
    blt         .Lcbc_tail4;

    sub         x4, x4, x5, LSR #2;         /* x4 - (4 * VL) */

    ld1b        {z15.b}, p0/z, [x2];
    ld1b        {z14.b}, p0/z, [x2, #1, MUL VL];
    ld1b        {z13.b}, p0/z, [x2, #2, MUL VL];
    ld1b        {z12.b}, p0/z, [x2, #3, MUL VL];
    rev         z0.b, z15.b;
    rev         z1.b, z14.b;
    rev         z2.b, z13.b;
    rev         z3.b, z12.b;
    rev         RTMP0.b, RIV.b;
    ext         z3.b, z3.b, z2.b, #16;
    ext         z2.b, z2.b, z1.b, #16;
    ext         z1.b, z1.b, z0.b, #16;
    ext         z0.b, z0.b, RTMP0.b, #16;
    rev         z3.b, z3.b;
    rev         z2.b, z2.b;
    rev         z1.b, z1.b;
    rev         z0.b, z0.b;
    mov         RIV.d, z12.d;

    SM4_SVE_CE_CRYPT_BLK4(z15, z14, z13, z12);

    eor         z0.d, z0.d, z15.d;
    eor         z1.d, z1.d, z14.d;
    eor         z2.d, z2.d, z13.d;
    eor         z3.d, z3.d, z12.d;
    st1b        {z0.b}, p0, [x1];
    st1b        {z1.b}, p0, [x1, #1, MUL VL];
    st1b        {z2.b}, p0, [x1, #2, MUL VL];
    st1b        {z3.b}, p0, [x1, #3, MUL VL];
    addvl       x2, x2, #4;
    addvl       x1, x1, #4;

    cbz         x4, .Lcbc_end;

.Lcbc_tail4:
    cmp         x4, x5, LSR #4;
    blt         .Lcbc_tail_ce;

    sub         x4, x4, x5, LSR #4;         /* x4 - VL */

    ld1b        {z15.b}, p0/z, [x2];
    rev         RTMP0.b, RIV.b;
    rev         z0.b, z15.b;
    ext         z0.b, z0.b, RTMP0.b, #16;
    rev         z0.b, z0.b;
    mov         RIV.d, z15.d;

    SM4_SVE_CE_CRYPT_BLK(z15);

    eor         z0.d, z0.d, z15.d;
    st1b        {z0.b}, p0, [x1];
    addvl       x2, x2, #1;
    addvl       x1, x1, #1;

    cbz         x4, .Lcbc_end;
    b           .Lcbc_tail4;

.Lcbc_tail_ce:
    rev         RIV.s, RIV.s;
    tbl         RIV.b, {RIV.b}, RSWAP128.b;

.Lcbc_tail:
    sub         x4, x4, #1;

    ld1         {v15.16b}, [x2], #16;
    mov         v0.16b, RIVv.16b;
    mov         RIVv.16b, v15.16b;
    SM4_CE_CRYPT_BLK(v15);
    eor         v0.16b, v0.16b, v15.16b;
    st1         {v0.16b}, [x1], #16;

    cbnz        x4, .Lcbc_tail;

    ext         RIV.b, RIV.b, RIV.b, #16;

.Lcbc_end:
    /* store new IV */
    rev         RIV.s, RIV.s;
    tbl         RIV.b, {RIV.b}, RSWAP128.b;
    st1         {RIVv.16b}, [x3];

    VPOP_ABI;
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv9_sve_ce_cbc_dec,.-_gcry_sm4_armv9_sve_ce_cbc_dec;)

.align 4
.global _gcry_sm4_armv9_sve_ce_cfb_dec
ELF(.type _gcry_sm4_armv9_sve_ce_cfb_dec,%function;)
_gcry_sm4_armv9_sve_ce_cfb_dec:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: iv (big endian, 128 bit)
     *   x4: nblocks
     */
    CFI_STARTPROC();
    VPUSH_ABI;

    PREPARE();
    ld1         {RIVv.16b}, [x3];
    ext         RIV.b, RIV.b, RIV.b, #16;

.Lcfb_loop_blks:
    sub         x4, x4, x5, LSR #1;         /* x4 - (8 * VL) */
    tbnz        x4, #63, .Lcfb_tail8;

    ld1b        {z15.b}, p0/z, [x2];
    ld1b        {z14.b}, p0/z, [x2, #1, MUL VL];
    ld1b        {z13.b}, p0/z, [x2, #2, MUL VL];
    ld1b        {z12.b}, p0/z, [x2, #3, MUL VL];
    ld1b        {z11.b}, p0/z, [x2, #4, MUL VL];
    ld1b        {z10.b}, p0/z, [x2, #5, MUL VL];
    ld1b        {z9.b}, p0/z, [x2, #6, MUL VL];
    ld1b        {z8.b}, p0/z, [x2, #7, MUL VL];
    rev         z0.b, z15.b;
    rev         z1.b, z14.b;
    rev         z2.b, z13.b;
    rev         z3.b, z12.b;
    rev         z4.b, z11.b;
    rev         z5.b, z10.b;
    rev         z6.b, z9.b;
    rev         z7.b, z8.b;
    rev         RTMP0.b, RIV.b;
    ext         z7.b, z7.b, z6.b, #16;
    ext         z6.b, z6.b, z5.b, #16;
    ext         z5.b, z5.b, z4.b, #16;
    ext         z4.b, z4.b, z3.b, #16;
    ext         z3.b, z3.b, z2.b, #16;
    ext         z2.b, z2.b, z1.b, #16;
    ext         z1.b, z1.b, z0.b, #16;
    ext         z0.b, z0.b, RTMP0.b, #16;
    rev         z7.b, z7.b;
    rev         z6.b, z6.b;
    rev         z5.b, z5.b;
    rev         z4.b, z4.b;
    rev         z3.b, z3.b;
    rev         z2.b, z2.b;
    rev         z1.b, z1.b;
    rev         z0.b, z0.b;
    mov         RIV.d, z8.d;

    SM4_SVE_CE_CRYPT_BLK8(z0, z1, z2, z3, z4, z5, z6, z7);

    eor         z0.d, z0.d, z15.d;
    eor         z1.d, z1.d, z14.d;
    eor         z2.d, z2.d, z13.d;
    eor         z3.d, z3.d, z12.d;
    eor         z4.d, z4.d, z11.d;
    eor         z5.d, z5.d, z10.d;
    eor         z6.d, z6.d, z9.d;
    eor         z7.d, z7.d, z8.d;
    st1b        {z0.b}, p0, [x1];
    st1b        {z1.b}, p0, [x1, #1, MUL VL];
    st1b        {z2.b}, p0, [x1, #2, MUL VL];
    st1b        {z3.b}, p0, [x1, #3, MUL VL];
    st1b        {z4.b}, p0, [x1, #4, MUL VL];
    st1b        {z5.b}, p0, [x1, #5, MUL VL];
    st1b        {z6.b}, p0, [x1, #6, MUL VL];
    st1b        {z7.b}, p0, [x1, #7, MUL VL];
    addvl       x2, x2, #8;
    addvl       x1, x1, #8;

    cbz         x4, .Lcfb_end;
    b           .Lcfb_loop_blks;

.Lcfb_tail8:
    add         x4, x4, x5, LSR #1;
    cmp         x4, x5, LSR #2;
    blt         .Lcfb_tail4;

    sub         x4, x4, x5, LSR #2;         /* x4 - (4 * VL) */

    ld1b        {z15.b}, p0/z, [x2];
    ld1b        {z14.b}, p0/z, [x2, #1, MUL VL];
    ld1b        {z13.b}, p0/z, [x2, #2, MUL VL];
    ld1b        {z12.b}, p0/z, [x2, #3, MUL VL];
    rev         z0.b, z15.b;
    rev         z1.b, z14.b;
    rev         z2.b, z13.b;
    rev         z3.b, z12.b;
    rev         RTMP0.b, RIV.b;
    ext         z3.b, z3.b, z2.b, #16;
    ext         z2.b, z2.b, z1.b, #16;
    ext         z1.b, z1.b, z0.b, #16;
    ext         z0.b, z0.b, RTMP0.b, #16;
    rev         z3.b, z3.b;
    rev         z2.b, z2.b;
    rev         z1.b, z1.b;
    rev         z0.b, z0.b;
    mov         RIV.d, z12.d;

    SM4_SVE_CE_CRYPT_BLK4(z0, z1, z2, z3);

    eor         z0.d, z0.d, z15.d;
    eor         z1.d, z1.d, z14.d;
    eor         z2.d, z2.d, z13.d;
    eor         z3.d, z3.d, z12.d;
    st1b        {z0.b}, p0, [x1];
    st1b        {z1.b}, p0, [x1, #1, MUL VL];
    st1b        {z2.b}, p0, [x1, #2, MUL VL];
    st1b        {z3.b}, p0, [x1, #3, MUL VL];
    addvl       x2, x2, #4;
    addvl       x1, x1, #4;

    cbz         x4, .Lcfb_end;

.Lcfb_tail4:
    cmp         x4, x5, LSR #4;
    blt         .Lcfb_tail_ce;

    sub         x4, x4, x5, LSR #4;         /* x4 - VL */

    ld1b        {z15.b}, p0/z, [x2];
    rev         RTMP0.b, RIV.b;
    rev         z0.b, z15.b;
    ext         z0.b, z0.b, RTMP0.b, #16;
    rev         z0.b, z0.b;
    mov         RIV.d, z15.d;

    SM4_SVE_CE_CRYPT_BLK(z0);

    eor         z0.d, z0.d, z15.d;
    st1b        {z0.b}, p0, [x1];
    addvl       x2, x2, #1;
    addvl       x1, x1, #1;

    cbz         x4, .Lcfb_end;
    b           .Lcfb_tail4;

.Lcfb_tail_ce:
    rev         RIV.s, RIV.s;
    tbl         RIV.b, {RIV.b}, RSWAP128.b;

.Lcfb_tail:
    sub         x4, x4, #1;

    ld1         {v15.16b}, [x2], #16;
    mov         v0.16b, RIVv.16b;
    mov         RIVv.16b, v15.16b;
    SM4_CE_CRYPT_BLK(v0);
    eor         v0.16b, v0.16b, v15.16b;
    st1         {v0.16b}, [x1], #16;

    cbnz        x4, .Lcfb_tail;

    ext         RIV.b, RIV.b, RIV.b, #16;

.Lcfb_end:
    /* store new IV */
    rev         RIV.s, RIV.s;
    tbl         RIV.b, {RIV.b}, RSWAP128.b;
    st1         {RIVv.16b}, [x3];

    VPOP_ABI;
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv9_sve_ce_cfb_dec,.-_gcry_sm4_armv9_sve_ce_cfb_dec;)

.align 4
.global _gcry_sm4_armv9_sve_ce_ctr_enc
ELF(.type _gcry_sm4_armv9_sve_ce_ctr_enc,%function;)
_gcry_sm4_armv9_sve_ce_ctr_enc:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: ctr (big endian, 128 bit)
     *   x4: nblocks
     */
    CFI_STARTPROC();

    PREPARE();

    dup         RZERO.d, #0;
    GET_DATA_POINTER(x6, .Lle128_inc);
    ld1b        {RLE128_INC.b}, p0/z, [x6];

    ldp         x7, x8, [x3];
    rev         x7, x7;
    rev         x8, x8;

#define inc_le128(zctr)                             \
        mov         RCTRv.d[1], x8;                 \
        mov         RCTRv.d[0], x7;                 \
        mov         zctr.d, RLE128_INC.d;           \
        dup         RCTR.q, RCTR.q[0];              \
        adds        x8, x8, x5, LSR #4;             \
        adc         x7, x7, xzr;                    \
        adclt       zctr.d, RCTR.d, RZERO.d;        \
        adclt       RCTR.d, zctr.d, RZERO.d;        \
        trn1        zctr.d, RCTR.d, zctr.d;         \
        revb        zctr.d, p0/m, zctr.d;

.Lctr_loop_blks:
    sub         x4, x4, x5, LSR #1;         /* x4 - (8 * VL) */
    tbnz        x4, #63, .Lctr_tail8;

    inc_le128(z0);
    inc_le128(z1);
    inc_le128(z2);
    inc_le128(z3);
    inc_le128(z4);
    inc_le128(z5);
    inc_le128(z6);
    inc_le128(z7);

    SM4_SVE_CE_CRYPT_BLK8(z0, z1, z2, z3, z4, z5, z6, z7);

    ld1b        {RTMP0.b}, p0/z, [x2];
    ld1b        {RTMP1.b}, p0/z, [x2, #1, MUL VL];
    ld1b        {RTMP2.b}, p0/z, [x2, #2, MUL VL];
    ld1b        {RTMP3.b}, p0/z, [x2, #3, MUL VL];
    eor         z0.d, z0.d, RTMP0.d;
    eor         z1.d, z1.d, RTMP1.d;
    eor         z2.d, z2.d, RTMP2.d;
    eor         z3.d, z3.d, RTMP3.d;
    ld1b        {RTMP0.b}, p0/z, [x2, #4, MUL VL];
    ld1b        {RTMP1.b}, p0/z, [x2, #5, MUL VL];
    ld1b        {RTMP2.b}, p0/z, [x2, #6, MUL VL];
    ld1b        {RTMP3.b}, p0/z, [x2, #7, MUL VL];
    eor         z4.d, z4.d, RTMP0.d;
    eor         z5.d, z5.d, RTMP1.d;
    eor         z6.d, z6.d, RTMP2.d;
    eor         z7.d, z7.d, RTMP3.d;
    addvl       x2, x2, #8;

    st1b        {z0.b}, p0, [x1];
    st1b        {z1.b}, p0, [x1, #1, MUL VL];
    st1b        {z2.b}, p0, [x1, #2, MUL VL];
    st1b        {z3.b}, p0, [x1, #3, MUL VL];
    st1b        {z4.b}, p0, [x1, #4, MUL VL];
    st1b        {z5.b}, p0, [x1, #5, MUL VL];
    st1b        {z6.b}, p0, [x1, #6, MUL VL];
    st1b        {z7.b}, p0, [x1, #7, MUL VL];
    addvl       x1, x1, #8;

    cbz         x4, .Lctr_end;
    b           .Lctr_loop_blks;

.Lctr_tail8:
    add         x4, x4, x5, LSR #1;
    cmp         x4, x5, LSR #2;
    blt         .Lctr_tail4;

    sub         x4, x4, x5, LSR #2;         /* x4 - (4 * VL) */

    inc_le128(z0);
    inc_le128(z1);
    inc_le128(z2);
    inc_le128(z3);

    SM4_SVE_CE_CRYPT_BLK4(z0, z1, z2, z3);

    ld1b        {RTMP0.b}, p0/z, [x2];
    ld1b        {RTMP1.b}, p0/z, [x2, #1, MUL VL];
    ld1b        {RTMP2.b}, p0/z, [x2, #2, MUL VL];
    ld1b        {RTMP3.b}, p0/z, [x2, #3, MUL VL];
    eor         z0.d, z0.d, RTMP0.d;
    eor         z1.d, z1.d, RTMP1.d;
    eor         z2.d, z2.d, RTMP2.d;
    eor         z3.d, z3.d, RTMP3.d;
    st1b        {z0.b}, p0, [x1];
    st1b        {z1.b}, p0, [x1, #1, MUL VL];
    st1b        {z2.b}, p0, [x1, #2, MUL VL];
    st1b        {z3.b}, p0, [x1, #3, MUL VL];
    addvl       x2, x2, #4;
    addvl       x1, x1, #4;

    cbz         x4, .Lctr_end;

.Lctr_tail4:
    cmp         x4, x5, LSR #4;
    blt         .Lctr_tail;

    sub         x4, x4, x5, LSR #4;         /* x4 - VL */

    inc_le128(z0);
    SM4_SVE_CE_CRYPT_BLK(z0);
    ld1b        {RTMP0.b}, p0/z, [x2];
    eor         z0.d, z0.d, RTMP0.d;
    st1b        {z0.b}, p0, [x1];
    addvl       x2, x2, #1;
    addvl       x1, x1, #1;

    cbz         x4, .Lctr_end;
    b           .Lctr_tail4;

.Lctr_tail:
    sub         x4, x4, #1;

    /* inc_le128 for CE */
    mov         v0.d[1], x8;
    mov         v0.d[0], x7;
    adds        x8, x8, #1;
    adc         x7, x7, xzr;
    rev64       v0.16b, v0.16b;

    SM4_CE_CRYPT_BLK(v0);
    ld1         {RTMP0v.16b}, [x2], #16;
    eor         v0.16b, v0.16b, RTMP0v.16b;
    st1         {v0.16b}, [x1], #16;

    cbnz        x4, .Lctr_tail;

.Lctr_end:
    /* store new CTR */
    rev x7, x7;
    rev x8, x8;
    stp x7, x8, [x3];

    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv9_sve_ce_ctr_enc,.-_gcry_sm4_armv9_sve_ce_ctr_enc;)

.align 4
.global _gcry_sm4_armv9_sve_get_vl
ELF(.type _gcry_sm4_armv9_sve_get_vl,%function;)
_gcry_sm4_armv9_sve_get_vl:
    CFI_STARTPROC();

    /* VL in bytes */
    rdvl        x0, #1;

    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv9_sve_get_vl,.-_gcry_sm4_armv9_sve_get_vl;)

#endif
