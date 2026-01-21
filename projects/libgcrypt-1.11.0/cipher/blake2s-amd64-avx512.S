/* blake2s-amd64-avx512.S  -  AVX512 implementation of BLAKE2s
 *
 * Copyright (C) 2022 Jussi Kivilinna <jussi.kivilinna@iki.fi>
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

/* The code is based on public-domain/CC0 BLAKE2 reference implementation
 * by Samual Neves, at https://github.com/BLAKE2/BLAKE2/tree/master/sse
 * Copyright 2012, Samuel Neves <sneves@dei.uc.pt>
 */

#ifdef __x86_64
#include <config.h>
#if defined(HAVE_GCC_INLINE_ASM_AVX512) && \
   (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
    defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))

#include "asm-common-amd64.h"

/* register macros */
#define RSTATE  %rdi
#define RINBLKS %rsi
#define RNBLKS  %rdx
#define RIV     %rcx

/* state structure */
#define STATE_H 0
#define STATE_T (STATE_H + 8 * 4)
#define STATE_F (STATE_T + 2 * 4)

/* vector registers */
#define ROW1  %xmm0
#define ROW2  %xmm1
#define ROW3  %xmm2
#define ROW4  %xmm3
#define TMP1  %xmm4
#define TMP1x %xmm4

#define MA1   %xmm5
#define MA2   %xmm6
#define MA3   %xmm7
#define MA4   %xmm8

#define MB1   %xmm9
#define MB2   %xmm10
#define MB3   %xmm11
#define MB4   %xmm12

/**********************************************************************
  blake2s/AVX
 **********************************************************************/

#define VPINSRD_KMASK(kpos, dpos, mem, vreg) \
        vmovdqu32 -((dpos) * 4) + mem, vreg {kpos}

#define GATHER_MSG(m1, m2, m3, m4, \
                   s0, s1, s2, s3, s4, s5, s6, s7, s8, \
                   s9, s10, s11, s12, s13, s14, s15) \
        vmovd (s0)*4(RINBLKS), m1; \
          vmovd (s1)*4(RINBLKS), m2; \
            vmovd (s8)*4(RINBLKS), m3; \
              vmovd (s9)*4(RINBLKS), m4; \
        vpinsrd $1, (s2)*4(RINBLKS), m1, m1; \
          vpinsrd $1, (s3)*4(RINBLKS), m2, m2; \
            vpinsrd $1, (s10)*4(RINBLKS), m3, m3; \
              vpinsrd $1, (s11)*4(RINBLKS), m4, m4; \
        vpinsrd $2, (s4)*4(RINBLKS), m1, m1; \
          vpinsrd $2, (s5)*4(RINBLKS), m2, m2; \
            vpinsrd $2, (s12)*4(RINBLKS), m3, m3; \
              vpinsrd $2, (s13)*4(RINBLKS), m4, m4; \
        vpinsrd $3, (s6)*4(RINBLKS), m1, m1; \
          vpinsrd $3, (s7)*4(RINBLKS), m2, m2; \
            vpinsrd $3, (s14)*4(RINBLKS), m3, m3; \
              vpinsrd $3, (s15)*4(RINBLKS), m4, m4;

#define GATHER_MSG_2(m1, m2, m3, m4, \
                   s0, s1, s2, s3, s4, s5, s6, s7, s8, \
                   s9, s10, s11, s12, s13, s14, s15) \
        vmovq (s0)*4(RINBLKS), m1; /* merged load */ \
          vmovd (s1)*4(RINBLKS), m2; \
            vmovd (s8)*4(RINBLKS), m3; \
              vmovd (s9)*4(RINBLKS), m4; \
          vpinsrd $1, (s3)*4(RINBLKS), m2, m2; \
            vpinsrd $1, (s10)*4(RINBLKS), m3, m3; \
              vpinsrd $1, (s11)*4(RINBLKS), m4, m4; \
        vpinsrd $2, (s4)*4(RINBLKS), m1, m1; \
          vpinsrd $2, (s5)*4(RINBLKS), m2, m2; \
            vpinsrd $2, (s12)*4(RINBLKS), m3, m3; \
              vpinsrd $2, (s13)*4(RINBLKS), m4, m4; \
        vpinsrd $3, (s6)*4(RINBLKS), m1, m1; \
          vpinsrd $3, (s7)*4(RINBLKS), m2, m2; \
            vpinsrd $3, (s14)*4(RINBLKS), m3, m3; \
              vpinsrd $3, (s15)*4(RINBLKS), m4, m4;

#define GATHER_MSG_3(m1, m2, m3, m4, \
                   s0, s1, s2, s3, s4, s5, s6, s7, s8, \
                   s9, s10, s11, s12, s13, s14, s15) \
        vmovd (s0)*4(RINBLKS), m1; \
          vmovd (s1)*4(RINBLKS), m2; \
            vmovdqu32 (s8)*4(RINBLKS), m3 {%k4}{z}; /* merged load */ \
              vmovd (s9)*4(RINBLKS), m4; \
        vpinsrd $1, (s2)*4(RINBLKS), m1, m1; \
          vpinsrd $1, (s3)*4(RINBLKS), m2, m2; \
            vpinsrd $1, (s10)*4(RINBLKS), m3, m3; \
              vpinsrd $1, (s11)*4(RINBLKS), m4, m4; \
        vpinsrd $2, (s4)*4(RINBLKS), m1, m1; \
          vpinsrd $2, (s5)*4(RINBLKS), m2, m2; \
            vpinsrd $2, (s12)*4(RINBLKS), m3, m3; \
              vpinsrd $2, (s13)*4(RINBLKS), m4, m4; \
        vpinsrd $3, (s6)*4(RINBLKS), m1, m1; \
          vpinsrd $3, (s7)*4(RINBLKS), m2, m2; \
            vpinsrd $3, (s14)*4(RINBLKS), m3, m3; \
              vpinsrd $3, (s15)*4(RINBLKS), m4, m4;

#define GATHER_MSG_5(m1, m2, m3, m4, \
                   s0, s1, s2, s3, s4, s5, s6, s7, s8, \
                   s9, s10, s11, s12, s13, s14, s15) \
        vmovd (s0)*4(RINBLKS), m1; \
          vmovd (s1)*4(RINBLKS), m2; \
            vmovd (s8)*4(RINBLKS), m3; \
              vmovd (s9)*4(RINBLKS), m4; \
        VPINSRD_KMASK(%k5, 1, (s2)*4(RINBLKS), m1); /* merged load */ \
          VPINSRD_KMASK(%k6, 1, (s3)*4(RINBLKS), m2); /* merged load */ \
            vpinsrd $1, (s10)*4(RINBLKS), m3, m3; \
              vpinsrd $1, (s11)*4(RINBLKS), m4, m4; \
        vpinsrd $2, (s4)*4(RINBLKS), m1, m1; \
            vpinsrd $2, (s12)*4(RINBLKS), m3, m3; \
              vpinsrd $2, (s13)*4(RINBLKS), m4, m4; \
          vpinsrd $3, (s7)*4(RINBLKS), m2, m2; \
            vpinsrd $3, (s14)*4(RINBLKS), m3, m3; \
              vpinsrd $3, (s15)*4(RINBLKS), m4, m4;

#define GATHER_MSG_6(m1, m2, m3, m4, \
                   s0, s1, s2, s3, s4, s5, s6, s7, s8, \
                   s9, s10, s11, s12, s13, s14, s15) \
        vmovdqu32 (s0)*4(RINBLKS), m1 {%k4}{z}; /* merged load */; \
          vmovd (s1)*4(RINBLKS), m2; \
            vmovd (s8)*4(RINBLKS), m3; \
              vmovd (s9)*4(RINBLKS), m4; \
        vpinsrd $1, (s2)*4(RINBLKS), m1, m1; \
          vpinsrd $1, (s3)*4(RINBLKS), m2, m2; \
            VPINSRD_KMASK(%k5, 1, (s10)*4(RINBLKS), m3); /* merged load */ \
              vpinsrd $1, (s11)*4(RINBLKS), m4, m4; \
          vpinsrd $2, (s5)*4(RINBLKS), m2, m2; \
            vpinsrd $2, (s12)*4(RINBLKS), m3, m3; \
              vpinsrd $2, (s13)*4(RINBLKS), m4, m4; \
        vpinsrd $3, (s6)*4(RINBLKS), m1, m1; \
          vpinsrd $3, (s7)*4(RINBLKS), m2, m2; \
              vpinsrd $3, (s15)*4(RINBLKS), m4, m4;

#define GATHER_MSG_8(m1, m2, m3, m4, \
                   s0, s1, s2, s3, s4, s5, s6, s7, s8, \
                   s9, s10, s11, s12, s13, s14, s15) \
        vmovd (s0)*4(RINBLKS), m1; \
          vmovd (s1)*4(RINBLKS), m2; \
            vmovq (s8)*4(RINBLKS), m3; \
              vmovd (s9)*4(RINBLKS), m4; \
        vpinsrd $1, (s2)*4(RINBLKS), m1, m1; \
          vpinsrd $1, (s3)*4(RINBLKS), m2, m2; \
              vpinsrd $1, (s11)*4(RINBLKS), m4, m4; \
        vpinsrd $2, (s4)*4(RINBLKS), m1, m1; \
          vpinsrd $2, (s5)*4(RINBLKS), m2, m2; \
            vpinsrd $2, (s12)*4(RINBLKS), m3, m3; \
              vpinsrq $1, (s13)*4(RINBLKS), m4, m4; \
        vpinsrd $3, (s6)*4(RINBLKS), m1, m1; \
          vpinsrd $3, (s7)*4(RINBLKS), m2, m2; \
            vpinsrd $3, (s14)*4(RINBLKS), m3, m3;

#define GATHER_MSG_9(m1, m2, m3, m4, \
                   s0, s1, s2, s3, s4, s5, s6, s7, s8, \
                   s9, s10, s11, s12, s13, s14, s15) \
        vmovd (s0)*4(RINBLKS), m1; \
          vmovdqu32 (s1)*4(RINBLKS), m2 {%k7}{z}; /* merged load */; \
            vmovd (s8)*4(RINBLKS), m3; \
              vmovd (s9)*4(RINBLKS), m4; \
        vpinsrd $1, (s2)*4(RINBLKS), m1, m1; \
          vpinsrd $1, (s3)*4(RINBLKS), m2, m2; \
            vpinsrd $1, (s10)*4(RINBLKS), m3, m3; \
              vpinsrd $1, (s11)*4(RINBLKS), m4, m4; \
        vpinsrd $2, (s4)*4(RINBLKS), m1, m1; \
          vpinsrd $2, (s5)*4(RINBLKS), m2, m2; \
            vpinsrd $2, (s12)*4(RINBLKS), m3, m3; \
              vpinsrd $2, (s13)*4(RINBLKS), m4, m4; \
        vpinsrd $3, (s6)*4(RINBLKS), m1, m1; \
            vpinsrd $3, (s14)*4(RINBLKS), m3, m3; \
              vpinsrd $3, (s15)*4(RINBLKS), m4, m4;

#define LOAD_MSG_0(m1, m2, m3, m4) \
        GATHER_MSG(m1, m2, m3, m4, \
                    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15)
#define LOAD_MSG_1(m1, m2, m3, m4) \
        GATHER_MSG(m1, m2, m3, m4, \
                   14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3)
#define LOAD_MSG_2(m1, m2, m3, m4) \
        GATHER_MSG_2(m1, m2, m3, m4, \
                     11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4)
#define LOAD_MSG_3(m1, m2, m3, m4) \
        GATHER_MSG_3(m1, m2, m3, m4, \
                     7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8)
#define LOAD_MSG_4(m1, m2, m3, m4) \
        GATHER_MSG(m1, m2, m3, m4, \
                    9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13)
#define LOAD_MSG_5(m1, m2, m3, m4) \
        GATHER_MSG_5(m1, m2, m3, m4, \
                     2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9)
#define LOAD_MSG_6(m1, m2, m3, m4) \
        GATHER_MSG_6(m1, m2, m3, m4, \
                     12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11)
#define LOAD_MSG_7(m1, m2, m3, m4) \
        GATHER_MSG(m1, m2, m3, m4, \
                   13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10)
#define LOAD_MSG_8(m1, m2, m3, m4) \
        GATHER_MSG_8(m1, m2, m3, m4, \
                     6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5)
#define LOAD_MSG_9(m1, m2, m3, m4) \
        GATHER_MSG_9(m1, m2, m3, m4, \
                     10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0)

#define LOAD_MSG(r, m1, m2, m3, m4) LOAD_MSG_##r(m1, m2, m3, m4)

#define ROR_16(in, out) vprord $16, in, out;

#define ROR_8(in, out)  vprord $8, in, out;

#define ROR_12(in, out) vprord $12, in, out;

#define ROR_7(in, out) vprord $7, in, out;

#define G(r1, r2, r3, r4, m, ROR_A, ROR_B) \
        vpaddd m, r1, r1; \
        vpaddd r2, r1, r1; \
        vpxor r1, r4, r4; \
        ROR_A(r4, r4); \
        vpaddd r4, r3, r3; \
        vpxor r3, r2, r2; \
        ROR_B(r2, r2);

#define G1(r1, r2, r3, r4, m) \
        G(r1, r2, r3, r4, m, ROR_16, ROR_12);

#define G2(r1, r2, r3, r4, m) \
        G(r1, r2, r3, r4, m, ROR_8, ROR_7);

#define MM_SHUFFLE(z,y,x,w) \
        (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))

#define DIAGONALIZE(r1, r2, r3, r4) \
        vpshufd $MM_SHUFFLE(0,3,2,1), r2, r2; \
        vpshufd $MM_SHUFFLE(1,0,3,2), r3, r3; \
        vpshufd $MM_SHUFFLE(2,1,0,3), r4, r4;

#define UNDIAGONALIZE(r1, r2, r3, r4) \
        vpshufd $MM_SHUFFLE(2,1,0,3), r2, r2; \
        vpshufd $MM_SHUFFLE(1,0,3,2), r3, r3; \
        vpshufd $MM_SHUFFLE(0,3,2,1), r4, r4;

#define ROUND(r, m1, m2, m3, m4) \
        G1(ROW1, ROW2, ROW3, ROW4, m1); \
        G2(ROW1, ROW2, ROW3, ROW4, m2); \
        DIAGONALIZE(ROW1, ROW2, ROW3, ROW4); \
        G1(ROW1, ROW2, ROW3, ROW4, m3); \
        G2(ROW1, ROW2, ROW3, ROW4, m4); \
        UNDIAGONALIZE(ROW1, ROW2, ROW3, ROW4);

SECTION_RODATA

ELF(.type _blake2s_avx512_data,@object;)
.align 16
_blake2s_avx512_data:
.Liv:
        .long 0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A
        .long 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
.Lk4_mask:
	.byte (1 << 0) + (1 << 2)
.Lk5_mask:
	.byte (1 << 1) + (1 << 3)
.Lk6_mask:
	.byte (1 << 1) + (1 << 2)
.Lk7_mask:
	.byte (1 << 0) + (1 << 3)

.text

.align 64
.globl _gcry_blake2s_transform_amd64_avx512
ELF(.type _gcry_blake2s_transform_amd64_avx512,@function;)

_gcry_blake2s_transform_amd64_avx512:
        /* input:
         *	%rdi: state
         *	%rsi: blks
         *	%rdx: num_blks
         */
        CFI_STARTPROC();

        spec_stop_avx512;

        kmovb .Lk4_mask rRIP, %k4;
        kmovb .Lk5_mask rRIP, %k5;
        kmovb .Lk6_mask rRIP, %k6;
        kmovb .Lk7_mask rRIP, %k7;

        addq $64, (STATE_T + 0)(RSTATE);

        vmovdqa .Liv+(0 * 4) rRIP, ROW3;
        vmovdqa .Liv+(4 * 4) rRIP, ROW4;

        vmovdqu (STATE_H + 0 * 4)(RSTATE), ROW1;
        vmovdqu (STATE_H + 4 * 4)(RSTATE), ROW2;

        vpxor (STATE_T)(RSTATE), ROW4, ROW4;

        LOAD_MSG(0, MA1, MA2, MA3, MA4);
        LOAD_MSG(1, MB1, MB2, MB3, MB4);
        jmp .Loop;

.align 64, 0xcc
.Loop:
        ROUND(0, MA1, MA2, MA3, MA4);
                                      LOAD_MSG(2, MA1, MA2, MA3, MA4);
        ROUND(1, MB1, MB2, MB3, MB4);
                                      LOAD_MSG(3, MB1, MB2, MB3, MB4);
        ROUND(2, MA1, MA2, MA3, MA4);
                                      LOAD_MSG(4, MA1, MA2, MA3, MA4);
        ROUND(3, MB1, MB2, MB3, MB4);
                                      LOAD_MSG(5, MB1, MB2, MB3, MB4);
        ROUND(4, MA1, MA2, MA3, MA4);
                                      LOAD_MSG(6, MA1, MA2, MA3, MA4);
        ROUND(5, MB1, MB2, MB3, MB4);
                                      LOAD_MSG(7, MB1, MB2, MB3, MB4);
        ROUND(6, MA1, MA2, MA3, MA4);
                                      LOAD_MSG(8, MA1, MA2, MA3, MA4);
        ROUND(7, MB1, MB2, MB3, MB4);
                                      LOAD_MSG(9, MB1, MB2, MB3, MB4);
        sub $1, RNBLKS;
        jz .Loop_end;

        lea 64(RINBLKS), RINBLKS;
        addq $64, (STATE_T + 0)(RSTATE);

        ROUND(8, MA1, MA2, MA3, MA4);
                                      LOAD_MSG(0, MA1, MA2, MA3, MA4);
        ROUND(9, MB1, MB2, MB3, MB4);
                                      LOAD_MSG(1, MB1, MB2, MB3, MB4);

        vpternlogq $0x96, (STATE_H + 0 * 4)(RSTATE), ROW3, ROW1;
        vpternlogq $0x96, (STATE_H + 4 * 4)(RSTATE), ROW4, ROW2;

        vmovdqa .Liv+(0 * 4) rRIP, ROW3;
        vmovdqa .Liv+(4 * 4) rRIP, ROW4;

        vmovdqu ROW1, (STATE_H + 0 * 4)(RSTATE);
        vmovdqu ROW2, (STATE_H + 4 * 4)(RSTATE);

        vpxor (STATE_T)(RSTATE), ROW4, ROW4;

        jmp .Loop;

.align 64, 0xcc
.Loop_end:
        ROUND(8, MA1, MA2, MA3, MA4);
        ROUND(9, MB1, MB2, MB3, MB4);

        vpternlogq $0x96, (STATE_H + 0 * 4)(RSTATE), ROW3, ROW1;
        vpternlogq $0x96, (STATE_H + 4 * 4)(RSTATE), ROW4, ROW2;

        vmovdqu ROW1, (STATE_H + 0 * 4)(RSTATE);
        vmovdqu ROW2, (STATE_H + 4 * 4)(RSTATE);

        xorl %eax, %eax;
        kxord %k4, %k4, %k4;
        kxord %k5, %k5, %k5;
        kxord %k6, %k6, %k6;
        kxord %k7, %k7, %k7;

        vzeroall;
        ret_spec_stop;
        CFI_ENDPROC();
ELF(.size _gcry_blake2s_transform_amd64_avx512,
    .-_gcry_blake2s_transform_amd64_avx512;)

#endif /*defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS)*/
#endif /*__x86_64*/
