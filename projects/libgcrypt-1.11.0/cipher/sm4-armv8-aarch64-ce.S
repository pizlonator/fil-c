/* sm4-armv8-aarch64-ce.S  -  ARMv8/AArch64/CE accelerated SM4 cipher
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
    defined(USE_SM4)

.cpu generic+simd+crypto

#define vecnum_v0 0
#define vecnum_v1 1
#define vecnum_v2 2
#define vecnum_v3 3
#define vecnum_v4 4
#define vecnum_v5 5
#define vecnum_v6 6
#define vecnum_v7 7
#define vecnum_v16 16
#define vecnum_v24 24
#define vecnum_v25 25
#define vecnum_v26 26
#define vecnum_v27 27
#define vecnum_v28 28
#define vecnum_v29 29
#define vecnum_v30 30
#define vecnum_v31 31

#define sm4e(vd, vn) \
   .inst (0xcec08400 | (vecnum_##vn << 5) | vecnum_##vd)

#define sm4ekey(vd, vn, vm) \
   .inst (0xce60c800 | (vecnum_##vm << 16) | (vecnum_##vn << 5) | vecnum_##vd)

.text

/* Register macros */

#define RTMP0   v16
#define RTMP1   v17
#define RTMP2   v18
#define RTMP3   v19

#define RIV     v20
#define RMASK   v21

/* Helper macros. */

#define load_rkey(ptr)                     \
        ld1 {v24.16b-v27.16b}, [ptr], #64; \
        ld1 {v28.16b-v31.16b}, [ptr];

#define SM4_CRYPT_BLK(b0)                       \
        rev32       b0.16b, b0.16b;             \
        sm4e(b0, v24);                          \
        sm4e(b0, v25);                          \
        sm4e(b0, v26);                          \
        sm4e(b0, v27);                          \
        sm4e(b0, v28);                          \
        sm4e(b0, v29);                          \
        sm4e(b0, v30);                          \
        sm4e(b0, v31);                          \
        rev64       b0.4s, b0.4s;               \
        ext         b0.16b, b0.16b, b0.16b, #8; \
        rev32       b0.16b, b0.16b;

#define crypt_blk4(b0, b1, b2, b3)         \
        rev32 b0.16b, b0.16b;              \
        rev32 b1.16b, b1.16b;              \
        rev32 b2.16b, b2.16b;              \
        rev32 b3.16b, b3.16b;              \
        sm4e(b0, v24);                     \
        sm4e(b1, v24);                     \
        sm4e(b2, v24);                     \
        sm4e(b3, v24);                     \
        sm4e(b0, v25);                     \
        sm4e(b1, v25);                     \
        sm4e(b2, v25);                     \
        sm4e(b3, v25);                     \
        sm4e(b0, v26);                     \
        sm4e(b1, v26);                     \
        sm4e(b2, v26);                     \
        sm4e(b3, v26);                     \
        sm4e(b0, v27);                     \
        sm4e(b1, v27);                     \
        sm4e(b2, v27);                     \
        sm4e(b3, v27);                     \
        sm4e(b0, v28);                     \
        sm4e(b1, v28);                     \
        sm4e(b2, v28);                     \
        sm4e(b3, v28);                     \
        sm4e(b0, v29);                     \
        sm4e(b1, v29);                     \
        sm4e(b2, v29);                     \
        sm4e(b3, v29);                     \
        sm4e(b0, v30);                     \
        sm4e(b1, v30);                     \
        sm4e(b2, v30);                     \
        sm4e(b3, v30);                     \
        sm4e(b0, v31);                     \
        sm4e(b1, v31);                     \
        sm4e(b2, v31);                     \
        sm4e(b3, v31);                     \
        rev64 b0.4s, b0.4s;                \
        rev64 b1.4s, b1.4s;                \
        rev64 b2.4s, b2.4s;                \
        rev64 b3.4s, b3.4s;                \
        ext b0.16b, b0.16b, b0.16b, #8;    \
        ext b1.16b, b1.16b, b1.16b, #8;    \
        ext b2.16b, b2.16b, b2.16b, #8;    \
        ext b3.16b, b3.16b, b3.16b, #8;    \
        rev32 b0.16b, b0.16b;              \
        rev32 b1.16b, b1.16b;              \
        rev32 b2.16b, b2.16b;              \
        rev32 b3.16b, b3.16b;

#define crypt_blk8(b0, b1, b2, b3, b4, b5, b6, b7) \
        rev32 b0.16b, b0.16b;              \
        rev32 b1.16b, b1.16b;              \
        rev32 b2.16b, b2.16b;              \
        rev32 b3.16b, b3.16b;              \
        rev32 b4.16b, b4.16b;              \
        rev32 b5.16b, b5.16b;              \
        rev32 b6.16b, b6.16b;              \
        rev32 b7.16b, b7.16b;              \
        sm4e(b0, v24);                     \
        sm4e(b1, v24);                     \
        sm4e(b2, v24);                     \
        sm4e(b3, v24);                     \
        sm4e(b4, v24);                     \
        sm4e(b5, v24);                     \
        sm4e(b6, v24);                     \
        sm4e(b7, v24);                     \
        sm4e(b0, v25);                     \
        sm4e(b1, v25);                     \
        sm4e(b2, v25);                     \
        sm4e(b3, v25);                     \
        sm4e(b4, v25);                     \
        sm4e(b5, v25);                     \
        sm4e(b6, v25);                     \
        sm4e(b7, v25);                     \
        sm4e(b0, v26);                     \
        sm4e(b1, v26);                     \
        sm4e(b2, v26);                     \
        sm4e(b3, v26);                     \
        sm4e(b4, v26);                     \
        sm4e(b5, v26);                     \
        sm4e(b6, v26);                     \
        sm4e(b7, v26);                     \
        sm4e(b0, v27);                     \
        sm4e(b1, v27);                     \
        sm4e(b2, v27);                     \
        sm4e(b3, v27);                     \
        sm4e(b4, v27);                     \
        sm4e(b5, v27);                     \
        sm4e(b6, v27);                     \
        sm4e(b7, v27);                     \
        sm4e(b0, v28);                     \
        sm4e(b1, v28);                     \
        sm4e(b2, v28);                     \
        sm4e(b3, v28);                     \
        sm4e(b4, v28);                     \
        sm4e(b5, v28);                     \
        sm4e(b6, v28);                     \
        sm4e(b7, v28);                     \
        sm4e(b0, v29);                     \
        sm4e(b1, v29);                     \
        sm4e(b2, v29);                     \
        sm4e(b3, v29);                     \
        sm4e(b4, v29);                     \
        sm4e(b5, v29);                     \
        sm4e(b6, v29);                     \
        sm4e(b7, v29);                     \
        sm4e(b0, v30);                     \
        sm4e(b1, v30);                     \
        sm4e(b2, v30);                     \
        sm4e(b3, v30);                     \
        sm4e(b4, v30);                     \
        sm4e(b5, v30);                     \
        sm4e(b6, v30);                     \
        sm4e(b7, v30);                     \
        sm4e(b0, v31);                     \
        sm4e(b1, v31);                     \
        sm4e(b2, v31);                     \
        sm4e(b3, v31);                     \
        sm4e(b4, v31);                     \
        sm4e(b5, v31);                     \
        sm4e(b6, v31);                     \
        sm4e(b7, v31);                     \
        rev64 b0.4s, b0.4s;                \
        rev64 b1.4s, b1.4s;                \
        rev64 b2.4s, b2.4s;                \
        rev64 b3.4s, b3.4s;                \
        rev64 b4.4s, b4.4s;                \
        rev64 b5.4s, b5.4s;                \
        rev64 b6.4s, b6.4s;                \
        rev64 b7.4s, b7.4s;                \
        ext b0.16b, b0.16b, b0.16b, #8;    \
        ext b1.16b, b1.16b, b1.16b, #8;    \
        ext b2.16b, b2.16b, b2.16b, #8;    \
        ext b3.16b, b3.16b, b3.16b, #8;    \
        ext b4.16b, b4.16b, b4.16b, #8;    \
        ext b5.16b, b5.16b, b5.16b, #8;    \
        ext b6.16b, b6.16b, b6.16b, #8;    \
        ext b7.16b, b7.16b, b7.16b, #8;    \
        rev32 b0.16b, b0.16b;              \
        rev32 b1.16b, b1.16b;              \
        rev32 b2.16b, b2.16b;              \
        rev32 b3.16b, b3.16b;              \
        rev32 b4.16b, b4.16b;              \
        rev32 b5.16b, b5.16b;              \
        rev32 b6.16b, b6.16b;              \
        rev32 b7.16b, b7.16b;


.align 4
.global _gcry_sm4_armv8_ce_expand_key
ELF(.type _gcry_sm4_armv8_ce_expand_key,%function;)
_gcry_sm4_armv8_ce_expand_key:
    /* input:
     *   x0: 128-bit key
     *   x1: rkey_enc
     *   x2: rkey_dec
     *   x3: fk array
     *   x4: ck array
     */
    CFI_STARTPROC();

    ld1 {v0.16b}, [x0];
    rev32 v0.16b, v0.16b;
    ld1 {v1.16b}, [x3];
    load_rkey(x4);

    /* input ^ fk */
    eor v0.16b, v0.16b, v1.16b;

    sm4ekey(v0, v0, v24);
    sm4ekey(v1, v0, v25);
    sm4ekey(v2, v1, v26);
    sm4ekey(v3, v2, v27);
    sm4ekey(v4, v3, v28);
    sm4ekey(v5, v4, v29);
    sm4ekey(v6, v5, v30);
    sm4ekey(v7, v6, v31);

    st1 {v0.16b-v3.16b}, [x1], #64;
    st1 {v4.16b-v7.16b}, [x1];
    rev64 v7.4s, v7.4s;
    rev64 v6.4s, v6.4s;
    rev64 v5.4s, v5.4s;
    rev64 v4.4s, v4.4s;
    rev64 v3.4s, v3.4s;
    rev64 v2.4s, v2.4s;
    rev64 v1.4s, v1.4s;
    rev64 v0.4s, v0.4s;
    ext v7.16b, v7.16b, v7.16b, #8;
    ext v6.16b, v6.16b, v6.16b, #8;
    ext v5.16b, v5.16b, v5.16b, #8;
    ext v4.16b, v4.16b, v4.16b, #8;
    ext v3.16b, v3.16b, v3.16b, #8;
    ext v2.16b, v2.16b, v2.16b, #8;
    ext v1.16b, v1.16b, v1.16b, #8;
    ext v0.16b, v0.16b, v0.16b, #8;
    st1 {v7.16b}, [x2], #16;
    st1 {v6.16b}, [x2], #16;
    st1 {v5.16b}, [x2], #16;
    st1 {v4.16b}, [x2], #16;
    st1 {v3.16b}, [x2], #16;
    st1 {v2.16b}, [x2], #16;
    st1 {v1.16b}, [x2], #16;
    st1 {v0.16b}, [x2];

    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv8_ce_expand_key,.-_gcry_sm4_armv8_ce_expand_key;)

.align 4
ELF(.type sm4_armv8_ce_crypt_blk1_4,%function;)
sm4_armv8_ce_crypt_blk1_4:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: num blocks (1..4)
     */
    CFI_STARTPROC();

    load_rkey(x0);

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
    crypt_blk4(v0, v1, v2, v3);

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
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size sm4_armv8_ce_crypt_blk1_4,.-sm4_armv8_ce_crypt_blk1_4;)

.align 4
.global _gcry_sm4_armv8_ce_crypt_blk1_8
ELF(.type _gcry_sm4_armv8_ce_crypt_blk1_8,%function;)
_gcry_sm4_armv8_ce_crypt_blk1_8:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: num blocks (1..8)
     */
    CFI_STARTPROC();

    cmp x3, #5;
    blt sm4_armv8_ce_crypt_blk1_4;

    load_rkey(x0);

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
    crypt_blk8(v0, v1, v2, v3, v4, v5, v6, v7);

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
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv8_ce_crypt_blk1_8,.-_gcry_sm4_armv8_ce_crypt_blk1_8;)

.align 4
.global _gcry_sm4_armv8_ce_crypt
ELF(.type _gcry_sm4_armv8_ce_crypt,%function;)
_gcry_sm4_armv8_ce_crypt:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: nblocks (multiples of 8)
     */
    CFI_STARTPROC();

    load_rkey(x0);

.Lcrypt_loop_blk:
    subs x3, x3, #8;
    bmi .Lcrypt_end;

    ld1 {v0.16b-v3.16b}, [x2], #64;
    ld1 {v4.16b-v7.16b}, [x2], #64;

    crypt_blk8(v0, v1, v2, v3, v4, v5, v6, v7);

    st1 {v0.16b-v3.16b}, [x1], #64;
    st1 {v4.16b-v7.16b}, [x1], #64;

    b .Lcrypt_loop_blk;

.Lcrypt_end:
    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv8_ce_crypt,.-_gcry_sm4_armv8_ce_crypt;)

.align 4
.global _gcry_sm4_armv8_ce_cbc_dec
ELF(.type _gcry_sm4_armv8_ce_cbc_dec,%function;)
_gcry_sm4_armv8_ce_cbc_dec:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: iv (big endian, 128 bit)
     *   x4: nblocks (multiples of 8)
     */
    CFI_STARTPROC();

    load_rkey(x0);
    ld1 {RIV.16b}, [x3];

.Lcbc_loop_blk:
    subs x4, x4, #8;
    bmi .Lcbc_end;

    ld1 {v0.16b-v3.16b}, [x2], #64;
    ld1 {v4.16b-v7.16b}, [x2];

    crypt_blk8(v0, v1, v2, v3, v4, v5, v6, v7);

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

    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv8_ce_cbc_dec,.-_gcry_sm4_armv8_ce_cbc_dec;)

.align 4
.global _gcry_sm4_armv8_ce_cfb_dec
ELF(.type _gcry_sm4_armv8_ce_cfb_dec,%function;)
_gcry_sm4_armv8_ce_cfb_dec:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: iv (big endian, 128 bit)
     *   x4: nblocks (multiples of 8)
     */
    CFI_STARTPROC();

    load_rkey(x0);
    ld1 {v0.16b}, [x3];

.Lcfb_loop_blk:
    subs x4, x4, #8;
    bmi .Lcfb_end;

    ld1 {v1.16b, v2.16b, v3.16b}, [x2], #48;
    ld1 {v4.16b-v7.16b}, [x2];

    crypt_blk8(v0, v1, v2, v3, v4, v5, v6, v7);

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

    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv8_ce_cfb_dec,.-_gcry_sm4_armv8_ce_cfb_dec;)

.align 4
.global _gcry_sm4_armv8_ce_ctr_enc
ELF(.type _gcry_sm4_armv8_ce_ctr_enc,%function;)
_gcry_sm4_armv8_ce_ctr_enc:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: ctr (big endian, 128 bit)
     *   x4: nblocks (multiples of 8)
     */
    CFI_STARTPROC();

    load_rkey(x0);

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

    crypt_blk8(v0, v1, v2, v3, v4, v5, v6, v7);

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

    ret_spec_stop;
    CFI_ENDPROC();
ELF(.size _gcry_sm4_armv8_ce_ctr_enc,.-_gcry_sm4_armv8_ce_ctr_enc;)

.align 4
.global _gcry_sm4_armv8_ce_xts_crypt
ELF(.type _gcry_sm4_armv8_ce_xts_crypt,%function;)
_gcry_sm4_armv8_ce_xts_crypt:
    /* input:
     *   x0: round key array, CTX
     *   x1: dst
     *   x2: src
     *   x3: tweak (big endian, 128 bit)
     *   x4: nblocks
     */
    CFI_STARTPROC()
    VPUSH_ABI

    load_rkey(x0)

    mov         x7, #0x87
    mov         x8, #0x1
    mov         RMASK.d[0], x7
    mov         RMASK.d[1], x8

    ld1         {RIV.16b}, [x3]
    mov         v8.16b, RIV.16b
    ext         RIV.16b, RIV.16b, RIV.16b, #8

.Lxts_loop_blk:
    sub         x4, x4, #8
    tbnz        x4, #63, .Lxts_tail8

#define tweak_next(vt, vin, RTMP)                       \
        sshr        RTMP.2d, RIV.2d, #63;               \
        add         vt.2d, vin.2d, vin.2d;              \
        and         RTMP.16b, RTMP.16b, RMASK.16b;      \
        add         RIV.2d, RIV.2d, RIV.2d;             \
        eor         vt.16b, vt.16b, RTMP.16b;

    tweak_next( v9,  v8, RTMP0)
    tweak_next(v10,  v9, RTMP1)
    tweak_next(v11, v10, RTMP2)
    tweak_next(v12, v11, RTMP3)
    tweak_next(v13, v12, RTMP0)
    tweak_next(v14, v13, RTMP1)
    tweak_next(v15, v14, RTMP2)

    ld1         {v0.16b-v3.16b}, [x2], #64
    eor         v0.16b, v0.16b,  v8.16b
    eor         v1.16b, v1.16b,  v9.16b
    eor         v2.16b, v2.16b, v10.16b
    eor         v3.16b, v3.16b, v11.16b
    ld1         {v4.16b-v7.16b}, [x2], #64
    eor         v4.16b, v4.16b, v12.16b
    eor         v5.16b, v5.16b, v13.16b
    eor         v6.16b, v6.16b, v14.16b
    eor         v7.16b, v7.16b, v15.16b

    crypt_blk8(v0, v1, v2, v3, v4, v5, v6, v7)

    eor         v0.16b, v0.16b,  v8.16b
    eor         v1.16b, v1.16b,  v9.16b
    eor         v2.16b, v2.16b, v10.16b
    eor         v3.16b, v3.16b, v11.16b
    st1         {v0.16b-v3.16b}, [x1], #64
    eor         v4.16b, v4.16b, v12.16b
    eor         v5.16b, v5.16b, v13.16b
    eor         v6.16b, v6.16b, v14.16b
    eor         v7.16b, v7.16b, v15.16b
    st1         {v4.16b-v7.16b}, [x1], #64

    tweak_next(v8, v15, RTMP3)

    cbz         x4, .Lxts_end
    b           .Lxts_loop_blk

.Lxts_tail8:
    add         x4, x4, #8
    cmp         x4, #4
    blt         .Lxts_tail4

    sub         x4, x4, #4

    tweak_next( v9,  v8, RTMP0)
    tweak_next(v10,  v9, RTMP1)
    tweak_next(v11, v10, RTMP2)

    ld1         {v0.16b-v3.16b}, [x2], #64
    eor         v0.16b, v0.16b,  v8.16b
    eor         v1.16b, v1.16b,  v9.16b
    eor         v2.16b, v2.16b, v10.16b
    eor         v3.16b, v3.16b, v11.16b

    crypt_blk4(v0, v1, v2, v3);

    eor         v0.16b, v0.16b,  v8.16b
    eor         v1.16b, v1.16b,  v9.16b
    eor         v2.16b, v2.16b, v10.16b
    eor         v3.16b, v3.16b, v11.16b
    st1         {v0.16b-v3.16b}, [x1], #64

    tweak_next(v8, v11, RTMP3)

    cbz         x4, .Lxts_end

.Lxts_tail4:
    sub         x4, x4, #1

    ld1         {v0.16b}, [x2], #16
    eor         v0.16b, v0.16b, v8.16b

    SM4_CRYPT_BLK(v0)

    eor         v0.16b, v0.16b, v8.16b
    st1         {v0.16b}, [x1], #16

    tweak_next(v8, v8, RTMP0)

    cbnz        x4, .Lxts_tail4

.Lxts_end:
    /* store new tweak */
    st1         {v8.16b}, [x3]

    CLEAR_REG(v8)
    CLEAR_REG(v9)
    CLEAR_REG(v10)
    CLEAR_REG(v11)
    CLEAR_REG(v12)
    CLEAR_REG(v13)
    CLEAR_REG(v14)
    CLEAR_REG(v15)
    CLEAR_REG(RIV)

    VPOP_ABI
    ret_spec_stop
    CFI_ENDPROC()
ELF(.size _gcry_sm4_armv8_ce_xts_crypt,.-_gcry_sm4_armv8_ce_xts_crypt;)

#endif
