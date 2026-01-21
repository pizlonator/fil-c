/* sha512-armv8-aarch64-ce.S - ARM/CE accelerated SHA-512 transform function
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

#include "asm-common-aarch64.h"

#if defined(__AARCH64EL__) && \
    defined(HAVE_COMPATIBLE_GCC_AARCH64_PLATFORM_AS) && \
    defined(HAVE_GCC_INLINE_ASM_AARCH64_SHA3_SHA512_SM3_SM4) && \
    defined(USE_SHA512)

.arch armv8.2-a+sha3+sm4

.text


/* Register macros */

#define Qv0 q0
#define Qv1 q1
#define Qv2 q2
#define Qv3 q3
#define Qv4 q4

#define vT0 v5
#define vT1 v6
#define QvT1 q6
#define vT2 v7
#define vT3 v16

#define vH01 v17
#define vH23 v18
#define vH45 v19
#define vH67 v20

#define vW0 v21
#define vW1 v22
#define vW2 v23
#define vW3 v24
#define vW4 v25
#define vW5 v26
#define vW6 v27
#define vW7 v28

#define vK0 v29
#define vK1 v30
#define vK2 v31


/* Round macros */

#define _(...) /*_*/

#define do_add(a, b) add a.2d, a.2d, b.2d;

#define load_k_3() ld1 {vK0.2d-vK2.2d}, [x3], #48;
#define load_k_last() ld1 {vK0.2d}, [x3];

#define load_msg1(...) \
        ld1 {vW0.16b-vW3.16b}, [x1], #64;

#define load_msg2(...) \
        rev64 vW0.16b, vW0.16b;

#define load_msg3(...) \
        rev64 vW1.16b, vW1.16b;

#define load_msg4(...) \
        ld1 {vW4.16b-vW7.16b}, [x1], #64;

#define load_msg5(...) \
        rev64 vW2.16b, vW2.16b;

#define load_msg6(...) \
        rev64 vW3.16b, vW3.16b;

#define load_msg7(...) \
        rev64 vW4.16b, vW4.16b;

#define load_msg8(...) \
        rev64 vW5.16b, vW5.16b;

#define load_msg9(...) \
        rev64 vW6.16b, vW6.16b;

#define load_msg10(...) \
        rev64 vW7.16b, vW7.16b;

#define schedule1(w0, w1, w2, w3, w4, w5, w6, w7) \
        sha512su0 w0.2d, w1.2d; \

#define schedule2(w0, w1, w2, w3, w4, w5, w6, w7) \
        ext vT2.16b, w4.16b, w5.16b, #8; \
        sha512su1 w0.2d, w7.2d, vT2.2d;

#define do_round2(ab, cd, ef, gh, cd_out, \
                  load_nextk_op, k, \
                  sched_op1, sched_op2, w0, w1, w2, w3, w4, w5, w6, w7) \
        add vT3.2d, k.2d, w0.2d; \
            load_nextk_op(); \
        ext vT1.16b, ef.16b, gh.16b, #8; \
        ext vT3.16b, vT3.16b, vT3.16b, #8; \
        ext vT0.16b, cd.16b, ef.16b, #8; \
        add gh.2d, gh.2d, vT3.2d; \
            sched_op1(w0, w1, w2, w3, w4, w5, w6, w7); \
        sha512h Q##gh, Q##vT1, vT0.2d; \
            sched_op2(w0, w1, w2, w3, w4, w5, w6, w7); \
        add cd_out.2d, gh.2d, cd.2d; \
        sha512h2 Q##gh, Q##cd, ab.2d; \


/* Other functional macros */

#undef CLEAR_REG
#define CLEAR_REG(reg, ...) movi reg.16b, #0;


/*
 * unsigned int
 * _gcry_sha512_transform_armv8_ce (u64 state[8], const void *input_data,
 *                                  size_t num_blks, const u64 k[80])
 */
.align 4
.globl _gcry_sha512_transform_armv8_ce
ELF(.type  _gcry_sha512_transform_armv8_ce,%function;)
_gcry_sha512_transform_armv8_ce:
  /* input:
   *	x0: ctx, CTX
   *	x1: data (128*nblks bytes)
   *	x2: nblks
   *	x3: k table
   */
  CFI_STARTPROC()

  cbz x2, .Ldo_nothing

  mov x4, x3

  ld1 {vH01.2d-vH67.2d}, [x0]  /* load state */

  load_msg1()
  mov v0.16b, vH01.16b
  mov v1.16b, vH23.16b
  load_k_3()
  load_msg2()
  load_msg3()
  load_msg4()
  mov v2.16b, vH45.16b
  mov v3.16b, vH67.16b
  load_msg5()
  load_msg6()
  load_msg7()
  load_msg8()
  load_msg9()
  load_msg10()

.Loop:
  sub x2, x2, #1

  # rounds 1-16
  do_round2(v0, v1, v2, v3, v4,
            _,        vK0,
            schedule1, schedule2, vW0, vW1, vW2, vW3, vW4, vW5, vW6, vW7)
  do_round2(v3, v0, v4, v2, v1,
            _,        vK1,
            schedule1, schedule2, vW1, vW2, vW3, vW4, vW5, vW6, vW7, vW0)
  do_round2(v2, v3, v1, v4, v0,
            load_k_3, vK2,
            schedule1, schedule2, vW2, vW3, vW4, vW5, vW6, vW7, vW0, vW1)
  do_round2(v4, v2, v0, v1, v3,
            _,        vK0,
            schedule1, schedule2, vW3, vW4, vW5, vW6, vW7, vW0, vW1, vW2)
  do_round2(v1, v4, v3, v0, v2,
            _,        vK1,
            schedule1, schedule2, vW4, vW5, vW6, vW7, vW0, vW1, vW2, vW3)
  do_round2(v0, v1, v2, v3, v4,
            load_k_3, vK2,
            schedule1, schedule2, vW5, vW6, vW7, vW0, vW1, vW2, vW3, vW4)
  do_round2(v3, v0, v4, v2, v1,
            _,        vK0,
            schedule1, schedule2, vW6, vW7, vW0, vW1, vW2, vW3, vW4, vW5)
  do_round2(v2, v3, v1, v4, v0,
            _,        vK1,
            schedule1, schedule2, vW7, vW0, vW1, vW2, vW3, vW4, vW5, vW6)

  # rounds 17-32
  do_round2(v4, v2, v0, v1, v3,
            load_k_3, vK2,
            schedule1, schedule2, vW0, vW1, vW2, vW3, vW4, vW5, vW6, vW7)
  do_round2(v1, v4, v3, v0, v2,
            _,        vK0,
            schedule1, schedule2, vW1, vW2, vW3, vW4, vW5, vW6, vW7, vW0)
  do_round2(v0, v1, v2, v3, v4,
            _,        vK1,
            schedule1, schedule2, vW2, vW3, vW4, vW5, vW6, vW7, vW0, vW1)
  do_round2(v3, v0, v4, v2, v1,
            load_k_3, vK2,
            schedule1, schedule2, vW3, vW4, vW5, vW6, vW7, vW0, vW1, vW2)
  do_round2(v2, v3, v1, v4, v0,
            _,        vK0,
            schedule1, schedule2, vW4, vW5, vW6, vW7, vW0, vW1, vW2, vW3)
  do_round2(v4, v2, v0, v1, v3,
            _,        vK1,
            schedule1, schedule2, vW5, vW6, vW7, vW0, vW1, vW2, vW3, vW4)
  do_round2(v1, v4, v3, v0, v2,
            load_k_3, vK2,
            schedule1, schedule2, vW6, vW7, vW0, vW1, vW2, vW3, vW4, vW5)
  do_round2(v0, v1, v2, v3, v4,
            _,        vK0,
            schedule1, schedule2, vW7, vW0, vW1, vW2, vW3, vW4, vW5, vW6)

  # rounds 33-48
  do_round2(v3, v0, v4, v2, v1,
            _,        vK1,
            schedule1, schedule2, vW0, vW1, vW2, vW3, vW4, vW5, vW6, vW7)
  do_round2(v2, v3, v1, v4, v0,
            load_k_3, vK2,
            schedule1, schedule2, vW1, vW2, vW3, vW4, vW5, vW6, vW7, vW0)
  do_round2(v4, v2, v0, v1, v3,
            _,        vK0,
            schedule1, schedule2, vW2, vW3, vW4, vW5, vW6, vW7, vW0, vW1)
  do_round2(v1, v4, v3, v0, v2,
            _,        vK1,
            schedule1, schedule2, vW3, vW4, vW5, vW6, vW7, vW0, vW1, vW2)
  do_round2(v0, v1, v2, v3, v4,
            load_k_3, vK2,
            schedule1, schedule2, vW4, vW5, vW6, vW7, vW0, vW1, vW2, vW3)
  do_round2(v3, v0, v4, v2, v1,
            _,        vK0,
            schedule1, schedule2, vW5, vW6, vW7, vW0, vW1, vW2, vW3, vW4)
  do_round2(v2, v3, v1, v4, v0,
            _,        vK1,
            schedule1, schedule2, vW6, vW7, vW0, vW1, vW2, vW3, vW4, vW5)
  do_round2(v4, v2, v0, v1, v3,
            load_k_3, vK2,
            schedule1, schedule2, vW7, vW0, vW1, vW2, vW3, vW4, vW5, vW6)

  # rounds 49-64
  do_round2(v1, v4, v3, v0, v2,
            _,        vK0,
            schedule1, schedule2, vW0, vW1, vW2, vW3, vW4, vW5, vW6, vW7)
  do_round2(v0, v1, v2, v3, v4,
            _,        vK1,
            schedule1, schedule2, vW1, vW2, vW3, vW4, vW5, vW6, vW7, vW0)
  do_round2(v3, v0, v4, v2, v1,
            load_k_3, vK2,
            schedule1, schedule2, vW2, vW3, vW4, vW5, vW6, vW7, vW0, vW1)
  do_round2(v2, v3, v1, v4, v0,
            _,        vK0,
            schedule1, schedule2, vW3, vW4, vW5, vW6, vW7, vW0, vW1, vW2)
  do_round2(v4, v2, v0, v1, v3,
            _,        vK1,
            schedule1, schedule2, vW4, vW5, vW6, vW7, vW0, vW1, vW2, vW3)
  do_round2(v1, v4, v3, v0, v2,
            load_k_3, vK2,
            schedule1, schedule2, vW5, vW6, vW7, vW0, vW1, vW2, vW3, vW4)
  do_round2(v0, v1, v2, v3, v4,
            _,        vK0,
            schedule1, schedule2, vW6, vW7, vW0, vW1, vW2, vW3, vW4, vW5)
  do_round2(v3, v0, v4, v2, v1,
            _,        vK1,
            schedule1, schedule2, vW7, vW0, vW1, vW2, vW3, vW4, vW5, vW6)

  cbz x2, .Lend

  # rounds 65-80
  do_round2(v2, v3, v1, v4, v0,
            load_k_3, vK2,
            _, _, vW0, , , , , , , )
  do_round2(v4, v2, v0, v1, v3,
            _,        vK0,
            _, _, vW1, , , , , , , )
  do_round2(v1, v4, v3, v0, v2,
            _,        vK1,
            _, _, vW2, , , , , , , )
  do_round2(v0, v1, v2, v3, v4,
            load_k_3, vK2,
            _, _, vW3, , , , , , , )
  do_round2(v3, v0, v4, v2, v1,
            _,        vK0,
            load_msg1, _, vW4, , , , , , , )
  do_round2(v2, v3, v1, v4, v0,
            _,        vK1,
            load_msg2, _, vW5, , , , , , , )
  do_round2(v4, v2, v0, v1, v3,
            load_k_last, vK2,
            load_msg3, _, vW6, , , , , , , )
  mov x3, x4
  do_round2(v1, v4, v3, v0, v2,
            load_k_3,    vK0,
            load_msg4, load_msg5, vW7, , , , , , , )

  load_msg6()
  load_msg7()

  add vH01.2d, vH01.2d, v0.2d
  add vH23.2d, vH23.2d, v1.2d
  add vH45.2d, vH45.2d, v2.2d
  add vH67.2d, vH67.2d, v3.2d
  load_msg8()
  load_msg9()
  load_msg10()
  mov v0.16b, vH01.16b
  mov v1.16b, vH23.16b
  mov v2.16b, vH45.16b
  mov v3.16b, vH67.16b

  b .Loop

.Lend:

  # rounds 65-80
  do_round2(v2, v3, v1, v4, v0,
            load_k_3, vK2,
            CLEAR_REG, _, vW0, , , , , , , )
  do_round2(v4, v2, v0, v1, v3,
            _,        vK0,
            CLEAR_REG, _, vW1, , , , , , , )
  do_round2(v1, v4, v3, v0, v2,
            _,        vK1,
            CLEAR_REG, _, vW2, , , , , , , )
  do_round2(v0, v1, v2, v3, v4,
            load_k_3, vK2,
            CLEAR_REG, _, vW3, , , , , , , )
  do_round2(v3, v0, v4, v2, v1,
            _,        vK0,
            CLEAR_REG, _, vW4, , , , , , , )
  do_round2(v2, v3, v1, v4, v0,
            _,        vK1,
            CLEAR_REG, _, vW5, , , , , , , )
  CLEAR_REG(vK1)
  do_round2(v4, v2, v0, v1, v3,
            load_k_last, vK2,
            CLEAR_REG, _, vW6, , , , , , , )
  CLEAR_REG(vK2)
  do_round2(v1, v4, v3, v0, v2,
            _,           vK0,
            CLEAR_REG, _, vW7, , , , , , , )
  CLEAR_REG(vK0)

  CLEAR_REG(v4)
  add vH01.2d, vH01.2d, v0.2d
  CLEAR_REG(v0)
  add vH23.2d, vH23.2d, v1.2d
  CLEAR_REG(v1)
  add vH45.2d, vH45.2d, v2.2d
  CLEAR_REG(v2)
  add vH67.2d, vH67.2d, v3.2d
  CLEAR_REG(v3)
  CLEAR_REG(vT0)
  CLEAR_REG(vT1)
  CLEAR_REG(vT2)
  CLEAR_REG(vT3)

  st1 {vH01.2d-vH67.2d}, [x0] /* store state */

  CLEAR_REG(vH01)
  CLEAR_REG(vH23)
  CLEAR_REG(vH45)
  CLEAR_REG(vH67)

.Ldo_nothing:
  mov x0, #0
  ret_spec_stop
  CFI_ENDPROC()
ELF(.size _gcry_sha512_transform_armv8_ce,.-_gcry_sha512_transform_armv8_ce;)

#endif
