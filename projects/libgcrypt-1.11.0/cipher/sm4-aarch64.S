/* sm4-aarch64.S  -  ARMv8/AArch64 accelerated SM4 cipher
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
    defined(HAVE_GCC_INLINE_ASM_AARCH64_NEON) && \
    defined(USE_SM4)

.cpu generic+simd

/* Constants */

SECTION_RODATA
.align 4
ELF(.type _gcry_sm4_aarch64_consts,@object)
_gcry_sm4_aarch64_consts:
.Lsm4_sbox:
  .byte 0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7
  .byte 0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05
  .byte 0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3
  .byte 0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99
  .byte 0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a
  .byte 0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62
  .byte 0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95
  .byte 0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6
  .byte 0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba
  .byte 0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8
  .byte 0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b
  .byte 0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35
  .byte 0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2
  .byte 0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87
  .byte 0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52
  .byte 0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e
  .byte 0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5
  .byte 0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1
  .byte 0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55
  .byte 0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3
  .byte 0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60
  .byte 0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f
  .byte 0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f
  .byte 0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51
  .byte 0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f
  .byte 0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8
  .byte 0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd
  .byte 0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0
  .byte 0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e
  .byte 0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84
  .byte 0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20
  .byte 0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48
ELF(.size _gcry_sm4_aarch64_consts,.-_gcry_sm4_aarch64_consts)

/* Register macros */

#define RTMP0   v8
#define RTMP1   v9
#define RTMP2   v10
#define RTMP3   v11

#define RX0     v12
#define RX1     v13
#define RKEY    v14
#define RIV     v15

/* Helper macros. */

#define preload_sbox(ptr)                   \
        GET_DATA_POINTER(ptr, .Lsm4_sbox);  \
        ld1 {v16.16b-v19.16b}, [ptr], #64;  \
        ld1 {v20.16b-v23.16b}, [ptr], #64;  \
        ld1 {v24.16b-v27.16b}, [ptr], #64;  \
        ld1 {v28.16b-v31.16b}, [ptr];

#define transpose_4x4(s0, s1, s2, s3)       \
        zip1 RTMP0.4s, s0.4s, s1.4s;        \
        zip1 RTMP1.4s, s2.4s, s3.4s;        \
        zip2 RTMP2.4s, s0.4s, s1.4s;        \
        zip2 RTMP3.4s, s2.4s, s3.4s;        \
        zip1 s0.2d, RTMP0.2d, RTMP1.2d;     \
        zip2 s1.2d, RTMP0.2d, RTMP1.2d;     \
        zip1 s2.2d, RTMP2.2d, RTMP3.2d;     \
        zip2 s3.2d, RTMP2.2d, RTMP3.2d;

#define rotate_clockwise_90(s0, s1, s2, s3) \
        zip1 RTMP0.4s, s1.4s, s0.4s;        \
        zip2 RTMP1.4s, s1.4s, s0.4s;        \
        zip1 RTMP2.4s, s3.4s, s2.4s;        \
        zip2 RTMP3.4s, s3.4s, s2.4s;        \
        zip1 s0.2d, RTMP2.2d, RTMP0.2d;     \
        zip2 s1.2d, RTMP2.2d, RTMP0.2d;     \
        zip1 s2.2d, RTMP3.2d, RTMP1.2d;     \
        zip2 s3.2d, RTMP3.2d, RTMP1.2d;


.text

.align 4
ELF(.type sm4_aarch64_crypt_blk1_4,%function;)
sm4_aarch64_crypt_blk1_4:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: num blocks (1..4)
     */
    CFI_STARTPROC();
    VPUSH_ABI;

    preload_sbox(x5);

    ld1 {v0.16b}, [x2], #16;
    mov v1.16b, v0.16b;
    mov v2.16b, v0.16b;
    mov v3.16b, v0.16b;
    cmp x3, #2;
    blt .Lblk4_load_input_done;
    ld1 {v1.16b}, [x2], #16;
    beq .Lblk4_load_input_done;
    ld1 {v2.16b}, [x2], #16;
    cmp x3, #3;
    beq .Lblk4_load_input_done;
    ld1 {v3.16b}, [x2];

.Lblk4_load_input_done:

    rev32 v0.16b, v0.16b;
    rev32 v1.16b, v1.16b;
    rev32 v2.16b, v2.16b;
    rev32 v3.16b, v3.16b;

    transpose_4x4(v0, v1, v2, v3);

#define ROUND(round, s0, s1, s2, s3)                     \
        dup RX0.4s, RKEY.s[round];                       \
        /* rk ^ s1 ^ s2 ^ s3 */                          \
        eor RTMP1.16b, s2.16b, s3.16b;                   \
        eor RX0.16b, RX0.16b, s1.16b;                    \
        eor RX0.16b, RX0.16b, RTMP1.16b;                 \
                                                         \
        /* sbox, non-linear part */                      \
        movi RTMP3.16b, #64;    /* sizeof(sbox) / 4 */   \
        tbl RTMP0.16b, {v16.16b-v19.16b}, RX0.16b;       \
        sub RX0.16b, RX0.16b, RTMP3.16b;                 \
        tbx RTMP0.16b, {v20.16b-v23.16b}, RX0.16b;       \
        sub RX0.16b, RX0.16b, RTMP3.16b;                 \
        tbx RTMP0.16b, {v24.16b-v27.16b}, RX0.16b;       \
        sub RX0.16b, RX0.16b, RTMP3.16b;                 \
        tbx RTMP0.16b, {v28.16b-v31.16b}, RX0.16b;       \
                                                         \
        /* linear part */                                \
        shl RTMP1.4s, RTMP0.4s, #8;                      \
        shl RTMP2.4s, RTMP0.4s, #16;                     \
        shl RTMP3.4s, RTMP0.4s, #24;                     \
        sri RTMP1.4s, RTMP0.4s, #(32-8);                 \
        sri RTMP2.4s, RTMP0.4s, #(32-16);                \
        sri RTMP3.4s, RTMP0.4s, #(32-24);                \
        /* RTMP1 = x ^ rol32(x, 8) ^ rol32(x, 16) */     \
        eor RTMP1.16b, RTMP1.16b, RTMP0.16b;             \
        eor RTMP1.16b, RTMP1.16b, RTMP2.16b;             \
        /* RTMP3 = x ^ rol32(x, 24) ^ rol32(RTMP1, 2) */ \
        eor RTMP3.16b, RTMP3.16b, RTMP0.16b;             \
        shl RTMP2.4s, RTMP1.4s, 2;                       \
        sri RTMP2.4s, RTMP1.4s, #(32-2);                 \
        eor RTMP3.16b, RTMP3.16b, RTMP2.16b;             \
        /* s0 ^= RTMP3 */                                \
        eor s0.16b, s0.16b, RTMP3.16b;

    mov x6, 8;
.Lroundloop4:
    ld1 {RKEY.4s}, [x0], #16;
    subs x6, x6, #1;

    ROUND(0, v0, v1, v2, v3);
    ROUND(1, v1, v2, v3, v0);
    ROUND(2, v2, v3, v0, v1);
    ROUND(3, v3, v0, v1, v2);

    bne .Lroundloop4;

#undef ROUND

    rotate_clockwise_90(v0, v1, v2, v3);
    rev32 v0.16b, v0.16b;
    rev32 v1.16b, v1.16b;
    rev32 v2.16b, v2.16b;
    rev32 v3.16b, v3.16b;

    st1 {v0.16b}, [x1], #16;
    cmp x3, #2;
    blt .Lblk4_store_output_done;
    st1 {v1.16b}, [x1], #16;
    beq .Lblk4_store_output_done;
    st1 {v2.16b}, [x1], #16;
    cmp x3, #3;
    beq .Lblk4_store_output_done;
    st1 {v3.16b}, [x1];

.Lblk4_store_output_done:
    VPOP_ABI;
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size sm4_aarch64_crypt_blk1_4,.-sm4_aarch64_crypt_blk1_4;)

.align 4
ELF(.type __sm4_crypt_blk8,%function;)
__sm4_crypt_blk8:
    /* input:
     *   x0: round key array, CTX
     *   v16-v31: fill with sbox
     *   v0, v1, v2, v3, v4, v5, v6, v7: eight parallel plaintext blocks
     * output:
     *   v0, v1, v2, v3, v4, v5, v6, v7: eight parallel ciphertext blocks
     */
    CFI_STARTPROC();

    rev32 v0.16b, v0.16b;
    rev32 v1.16b, v1.16b;
    rev32 v2.16b, v2.16b;
    rev32 v3.16b, v3.16b;
    rev32 v4.16b, v4.16b;
    rev32 v5.16b, v5.16b;
    rev32 v6.16b, v6.16b;
    rev32 v7.16b, v7.16b;

    transpose_4x4(v0, v1, v2, v3);
    transpose_4x4(v4, v5, v6, v7);

#define ROUND(round, s0, s1, s2, s3, t0, t1, t2, t3)     \
        /* rk ^ s1 ^ s2 ^ s3 */                          \
        dup RX0.4s, RKEY.s[round];                       \
        eor RTMP0.16b, s2.16b, s3.16b;                   \
        mov RX1.16b, RX0.16b;                            \
        eor RTMP1.16b, t2.16b, t3.16b;                   \
        eor RX0.16b, RX0.16b, s1.16b;                    \
        eor RX1.16b, RX1.16b, t1.16b;                    \
        eor RX0.16b, RX0.16b, RTMP0.16b;                 \
        eor RX1.16b, RX1.16b, RTMP1.16b;                 \
                                                         \
        /* sbox, non-linear part */                      \
        movi RTMP3.16b, #64;    /* sizeof(sbox) / 4 */   \
        tbl RTMP0.16b, {v16.16b-v19.16b}, RX0.16b;       \
        tbl RTMP1.16b, {v16.16b-v19.16b}, RX1.16b;       \
        sub RX0.16b, RX0.16b, RTMP3.16b;                 \
        sub RX1.16b, RX1.16b, RTMP3.16b;                 \
        tbx RTMP0.16b, {v20.16b-v23.16b}, RX0.16b;       \
        tbx RTMP1.16b, {v20.16b-v23.16b}, RX1.16b;       \
        sub RX0.16b, RX0.16b, RTMP3.16b;                 \
        sub RX1.16b, RX1.16b, RTMP3.16b;                 \
        tbx RTMP0.16b, {v24.16b-v27.16b}, RX0.16b;       \
        tbx RTMP1.16b, {v24.16b-v27.16b}, RX1.16b;       \
        sub RX0.16b, RX0.16b, RTMP3.16b;                 \
        sub RX1.16b, RX1.16b, RTMP3.16b;                 \
        tbx RTMP0.16b, {v28.16b-v31.16b}, RX0.16b;       \
        tbx RTMP1.16b, {v28.16b-v31.16b}, RX1.16b;       \
                                                         \
        /* linear part */                                \
        shl RX0.4s, RTMP0.4s, #8;                        \
        shl RX1.4s, RTMP1.4s, #8;                        \
        shl RTMP2.4s, RTMP0.4s, #16;                     \
        shl RTMP3.4s, RTMP1.4s, #16;                     \
        sri RX0.4s, RTMP0.4s, #(32 - 8);                 \
        sri RX1.4s, RTMP1.4s, #(32 - 8);                 \
        sri RTMP2.4s, RTMP0.4s, #(32 - 16);              \
        sri RTMP3.4s, RTMP1.4s, #(32 - 16);              \
        /* RX = x ^ rol32(x, 8) ^ rol32(x, 16) */        \
        eor RX0.16b, RX0.16b, RTMP0.16b;                 \
        eor RX1.16b, RX1.16b, RTMP1.16b;                 \
        eor RX0.16b, RX0.16b, RTMP2.16b;                 \
        eor RX1.16b, RX1.16b, RTMP3.16b;                 \
        /* RTMP0/1 ^= x ^ rol32(x, 24) ^ rol32(RX, 2) */ \
        shl RTMP2.4s, RTMP0.4s, #24;                     \
        shl RTMP3.4s, RTMP1.4s, #24;                     \
        sri RTMP2.4s, RTMP0.4s, #(32 - 24);              \
        sri RTMP3.4s, RTMP1.4s, #(32 - 24);              \
        eor RTMP0.16b, RTMP0.16b, RTMP2.16b;             \
        eor RTMP1.16b, RTMP1.16b, RTMP3.16b;             \
        shl RTMP2.4s, RX0.4s, #2;                        \
        shl RTMP3.4s, RX1.4s, #2;                        \
        sri RTMP2.4s, RX0.4s, #(32 - 2);                 \
        sri RTMP3.4s, RX1.4s, #(32 - 2);                 \
        eor RTMP0.16b, RTMP0.16b, RTMP2.16b;             \
        eor RTMP1.16b, RTMP1.16b, RTMP3.16b;             \
        /* s0/t0 ^= RTMP0/1 */                           \
        eor s0.16b, s0.16b, RTMP0.16b;                   \
        eor t0.16b, t0.16b, RTMP1.16b;

    mov x6, 8;
.Lroundloop8:
    ld1 {RKEY.4s}, [x0], #16;
    subs x6, x6, #1;

    ROUND(0, v0, v1, v2, v3, v4, v5, v6, v7);
    ROUND(1, v1, v2, v3, v0, v5, v6, v7, v4);
    ROUND(2, v2, v3, v0, v1, v6, v7, v4, v5);
    ROUND(3, v3, v0, v1, v2, v7, v4, v5, v6);

    bne .Lroundloop8;

#undef ROUND

    rotate_clockwise_90(v0, v1, v2, v3);
    rotate_clockwise_90(v4, v5, v6, v7);
    rev32 v0.16b, v0.16b;
    rev32 v1.16b, v1.16b;
    rev32 v2.16b, v2.16b;
    rev32 v3.16b, v3.16b;
    rev32 v4.16b, v4.16b;
    rev32 v5.16b, v5.16b;
    rev32 v6.16b, v6.16b;
    rev32 v7.16b, v7.16b;

    sub x0, x0, #128;       /* repoint to rkey */
    ret;
    CFI_ENDPROC();
ELF(.size __sm4_crypt_blk8,.-__sm4_crypt_blk8;)

.align 4
.global _gcry_sm4_aarch64_crypt_blk1_8
ELF(.type _gcry_sm4_aarch64_crypt_blk1_8,%function;)
_gcry_sm4_aarch64_crypt_blk1_8:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: num blocks (1..8)
     */
    CFI_STARTPROC();

    cmp x3, #5;
    blt sm4_aarch64_crypt_blk1_4;

    stp x29, x30, [sp, #-16]!;
    CFI_ADJUST_CFA_OFFSET(16);
    CFI_REG_ON_STACK(29, 0);
    CFI_REG_ON_STACK(30, 8);
    VPUSH_ABI;

    preload_sbox(x5);

    ld1 {v0.16b-v3.16b}, [x2], #64;
    ld1 {v4.16b}, [x2], #16;
    mov v5.16b, v4.16b;
    mov v6.16b, v4.16b;
    mov v7.16b, v4.16b;
    beq .Lblk8_load_input_done;
    ld1 {v5.16b}, [x2], #16;
    cmp x3, #7;
    blt .Lblk8_load_input_done;
    ld1 {v6.16b}, [x2], #16;
    beq .Lblk8_load_input_done;
    ld1 {v7.16b}, [x2];

.Lblk8_load_input_done:
    bl __sm4_crypt_blk8;

    cmp x3, #6;
    st1 {v0.16b-v3.16b}, [x1], #64;
    st1 {v4.16b}, [x1], #16;
    blt .Lblk8_store_output_done;
    st1 {v5.16b}, [x1], #16;
    beq .Lblk8_store_output_done;
    st1 {v6.16b}, [x1], #16;
    cmp x3, #7;
    beq .Lblk8_store_output_done;
    st1 {v7.16b}, [x1];

.Lblk8_store_output_done:
    VPOP_ABI;
    ldp x29, x30, [sp], #16;
    CFI_ADJUST_CFA_OFFSET(-16);
    CFI_RESTORE(x29);
    CFI_RESTORE(x30);
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_aarch64_crypt_blk1_8,.-_gcry_sm4_aarch64_crypt_blk1_8;)


.align 4
.global _gcry_sm4_aarch64_crypt
ELF(.type _gcry_sm4_aarch64_crypt,%function;)
_gcry_sm4_aarch64_crypt:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: nblocks (multiples of 8)
     */
    CFI_STARTPROC();

    stp x29, x30, [sp, #-16]!;
    CFI_ADJUST_CFA_OFFSET(16);
    CFI_REG_ON_STACK(29, 0);
    CFI_REG_ON_STACK(30, 8);
    VPUSH_ABI;

    preload_sbox(x5);

.Lcrypt_loop_blk:
    subs x3, x3, #8;
    bmi .Lcrypt_end;

    ld1 {v0.16b-v3.16b}, [x2], #64;
    ld1 {v4.16b-v7.16b}, [x2], #64;
    bl __sm4_crypt_blk8;
    st1 {v0.16b-v3.16b}, [x1], #64;
    st1 {v4.16b-v7.16b}, [x1], #64;
    b .Lcrypt_loop_blk;

.Lcrypt_end:
    VPOP_ABI;
    ldp x29, x30, [sp], #16;
    CFI_ADJUST_CFA_OFFSET(-16);
    CFI_RESTORE(x29);
    CFI_RESTORE(x30);
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_aarch64_crypt,.-_gcry_sm4_aarch64_crypt;)


.align 4
.global _gcry_sm4_aarch64_cbc_dec
ELF(.type _gcry_sm4_aarch64_cbc_dec,%function;)
_gcry_sm4_aarch64_cbc_dec:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: iv (big endian, 128 bit)
     *   x4: nblocks (multiples of 8)
     */
    CFI_STARTPROC();

    stp x29, x30, [sp, #-16]!;
    CFI_ADJUST_CFA_OFFSET(16);
    CFI_REG_ON_STACK(29, 0);
    CFI_REG_ON_STACK(30, 8);
    VPUSH_ABI;

    preload_sbox(x5);
    ld1 {RIV.16b}, [x3];

.Lcbc_loop_blk:
    subs x4, x4, #8;
    bmi .Lcbc_end;

    ld1 {v0.16b-v3.16b}, [x2], #64;
    ld1 {v4.16b-v7.16b}, [x2];

    bl __sm4_crypt_blk8;

    sub x2, x2, #64;
    eor v0.16b, v0.16b, RIV.16b;
    ld1 {RTMP0.16b-RTMP3.16b}, [x2], #64;
    eor v1.16b, v1.16b, RTMP0.16b;
    eor v2.16b, v2.16b, RTMP1.16b;
    eor v3.16b, v3.16b, RTMP2.16b;
    st1 {v0.16b-v3.16b}, [x1], #64;

    eor v4.16b, v4.16b, RTMP3.16b;
    ld1 {RTMP0.16b-RTMP3.16b}, [x2], #64;
    eor v5.16b, v5.16b, RTMP0.16b;
    eor v6.16b, v6.16b, RTMP1.16b;
    eor v7.16b, v7.16b, RTMP2.16b;

    mov RIV.16b, RTMP3.16b;
    st1 {v4.16b-v7.16b}, [x1], #64;

    b .Lcbc_loop_blk;

.Lcbc_end:
    /* store new IV */
    st1 {RIV.16b}, [x3];

    VPOP_ABI;
    ldp x29, x30, [sp], #16;
    CFI_ADJUST_CFA_OFFSET(-16);
    CFI_RESTORE(x29);
    CFI_RESTORE(x30);
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_aarch64_cbc_dec,.-_gcry_sm4_aarch64_cbc_dec;)

.align 4
.global _gcry_sm4_aarch64_cfb_dec
ELF(.type _gcry_sm4_aarch64_cfb_dec,%function;)
_gcry_sm4_aarch64_cfb_dec:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: iv (big endian, 128 bit)
     *   x4: nblocks (multiples of 8)
     */
    CFI_STARTPROC();

    stp x29, x30, [sp, #-16]!;
    CFI_ADJUST_CFA_OFFSET(16);
    CFI_REG_ON_STACK(29, 0);
    CFI_REG_ON_STACK(30, 8);
    VPUSH_ABI;

    preload_sbox(x5);
    ld1 {v0.16b}, [x3];

.Lcfb_loop_blk:
    subs x4, x4, #8;
    bmi .Lcfb_end;

    ld1 {v1.16b, v2.16b, v3.16b}, [x2], #48;
    ld1 {v4.16b-v7.16b}, [x2];

    bl __sm4_crypt_blk8;

    sub x2, x2, #48;
    ld1 {RTMP0.16b-RTMP3.16b}, [x2], #64;
    eor v0.16b, v0.16b, RTMP0.16b;
    eor v1.16b, v1.16b, RTMP1.16b;
    eor v2.16b, v2.16b, RTMP2.16b;
    eor v3.16b, v3.16b, RTMP3.16b;
    st1 {v0.16b-v3.16b}, [x1], #64;

    ld1 {RTMP0.16b-RTMP3.16b}, [x2], #64;
    eor v4.16b, v4.16b, RTMP0.16b;
    eor v5.16b, v5.16b, RTMP1.16b;
    eor v6.16b, v6.16b, RTMP2.16b;
    eor v7.16b, v7.16b, RTMP3.16b;
    st1 {v4.16b-v7.16b}, [x1], #64;

    mov v0.16b, RTMP3.16b;

    b .Lcfb_loop_blk;

.Lcfb_end:
    /* store new IV */
    st1 {v0.16b}, [x3];

    VPOP_ABI;
    ldp x29, x30, [sp], #16;
    CFI_ADJUST_CFA_OFFSET(-16);
    CFI_RESTORE(x29);
    CFI_RESTORE(x30);
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_aarch64_cfb_dec,.-_gcry_sm4_aarch64_cfb_dec;)

.align 4
.global _gcry_sm4_aarch64_ctr_enc
ELF(.type _gcry_sm4_aarch64_ctr_enc,%function;)
_gcry_sm4_aarch64_ctr_enc:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: ctr (big endian, 128 bit)
     *   x4: nblocks (multiples of 8)
     */
    CFI_STARTPROC();

    stp x29, x30, [sp, #-16]!;
    CFI_ADJUST_CFA_OFFSET(16);
    CFI_REG_ON_STACK(29, 0);
    CFI_REG_ON_STACK(30, 8);
    VPUSH_ABI;

    preload_sbox(x5);

    ldp x7, x8, [x3];
    rev x7, x7;
    rev x8, x8;

.Lctr_loop_blk:
    subs x4, x4, #8;
    bmi .Lctr_end;

#define inc_le128(vctr)       \
    mov vctr.d[1], x8;        \
    mov vctr.d[0], x7;        \
    adds x8, x8, #1;          \
    adc x7, x7, xzr;          \
    rev64 vctr.16b, vctr.16b;

    /* construct CTRs */
    inc_le128(v0);      /* +0 */
    inc_le128(v1);      /* +1 */
    inc_le128(v2);      /* +2 */
    inc_le128(v3);      /* +3 */
    inc_le128(v4);      /* +4 */
    inc_le128(v5);      /* +5 */
    inc_le128(v6);      /* +6 */
    inc_le128(v7);      /* +7 */

    bl __sm4_crypt_blk8;

    ld1 {RTMP0.16b-RTMP3.16b}, [x2], #64;
    eor v0.16b, v0.16b, RTMP0.16b;
    eor v1.16b, v1.16b, RTMP1.16b;
    eor v2.16b, v2.16b, RTMP2.16b;
    eor v3.16b, v3.16b, RTMP3.16b;
    st1 {v0.16b-v3.16b}, [x1], #64;

    ld1 {RTMP0.16b-RTMP3.16b}, [x2], #64;
    eor v4.16b, v4.16b, RTMP0.16b;
    eor v5.16b, v5.16b, RTMP1.16b;
    eor v6.16b, v6.16b, RTMP2.16b;
    eor v7.16b, v7.16b, RTMP3.16b;
    st1 {v4.16b-v7.16b}, [x1], #64;

    b .Lctr_loop_blk;

.Lctr_end:
    /* store new CTR */
    rev x7, x7;
    rev x8, x8;
    stp x7, x8, [x3];

    VPOP_ABI;
    ldp x29, x30, [sp], #16;
    CFI_ADJUST_CFA_OFFSET(-16);
    CFI_RESTORE(x29);
    CFI_RESTORE(x30);
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_aarch64_ctr_enc,.-_gcry_sm4_aarch64_ctr_enc;)

#endif
