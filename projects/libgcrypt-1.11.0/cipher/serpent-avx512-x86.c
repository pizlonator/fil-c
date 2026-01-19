/* serpent-avx512-x86.c  -  AVX512 implementation of Serpent cipher
 *
 * Copyright (C) 2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
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

#if defined(__x86_64) || defined(__i386)
#if defined(HAVE_COMPATIBLE_CC_X86_AVX512_INTRINSICS) && \
    defined(USE_SERPENT) && defined(ENABLE_AVX512_SUPPORT)

#include <immintrin.h>
#include <string.h>
#include <stdio.h>

#include "g10lib.h"
#include "types.h"
#include "cipher.h"
#include "bithelp.h"
#include "bufhelp.h"
#include "cipher-internal.h"
#include "bulkhelp.h"

#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))

/* Number of rounds per Serpent encrypt/decrypt operation.  */
#define ROUNDS 32

/* Serpent works on 128 bit blocks.  */
typedef unsigned int serpent_block_t[4];

/* The key schedule consists of 33 128 bit subkeys.  */
typedef unsigned int serpent_subkeys_t[ROUNDS + 1][4];

#define vpunpckhdq(a, b, o)  ((o) = _mm512_unpackhi_epi32((b), (a)))
#define vpunpckldq(a, b, o)  ((o) = _mm512_unpacklo_epi32((b), (a)))
#define vpunpckhqdq(a, b, o) ((o) = _mm512_unpackhi_epi64((b), (a)))
#define vpunpcklqdq(a, b, o) ((o) = _mm512_unpacklo_epi64((b), (a)))

#define vpbroadcastd(v) _mm512_set1_epi32(v)

#define vrol(x, s) _mm512_rol_epi32((x), (s))
#define vror(x, s) _mm512_ror_epi32((x), (s))
#define vshl(x, s) _mm512_slli_epi32((x), (s))

/* 4x4 32-bit integer matrix transpose */
#define transpose_4x4(x0, x1, x2, x3, t1, t2, t3) \
	vpunpckhdq(x1, x0, t2); \
	vpunpckldq(x1, x0, x0); \
	\
	vpunpckldq(x3, x2, t1); \
	vpunpckhdq(x3, x2, x2); \
	\
	vpunpckhqdq(t1, x0, x1); \
	vpunpcklqdq(t1, x0, x0); \
	\
	vpunpckhqdq(x2, t2, x3); \
	vpunpcklqdq(x2, t2, x2);

/*
 * These are the S-Boxes of Serpent from following research paper.
 *
 *  D. A. Osvik, “Speeding up Serpent,” in Third AES Candidate Conference,
 *   (New York, New York, USA), p. 317–329, National Institute of Standards and
 *   Technology, 2000.
 *
 * Paper is also available at: http://www.ii.uib.no/~osvik/pub/aes3.pdf
 *
 * --
 *
 * Following logic gets heavily optimized by compiler to use AVX512F
 * 'vpternlogq' instruction. This gives higher performance increase than
 * would be expected from simple wideing of vectors from AVX2/256bit to
 * AVX512/512bit.
 *
 */

#define SBOX0(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r3 ^= r0; r4 =  r1; \
    r1 &= r3; r4 ^= r2; \
    r1 ^= r0; r0 |= r3; \
    r0 ^= r4; r4 ^= r3; \
    r3 ^= r2; r2 |= r1; \
    r2 ^= r4; r4 = ~r4; \
    r4 |= r1; r1 ^= r3; \
    r1 ^= r4; r3 |= r0; \
    r1 ^= r3; r4 ^= r3; \
    \
    w = r1; x = r4; y = r2; z = r0; \
  }

#define SBOX0_INVERSE(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r2 = ~r2; r4 =  r1; \
    r1 |= r0; r4 = ~r4; \
    r1 ^= r2; r2 |= r4; \
    r1 ^= r3; r0 ^= r4; \
    r2 ^= r0; r0 &= r3; \
    r4 ^= r0; r0 |= r1; \
    r0 ^= r2; r3 ^= r4; \
    r2 ^= r1; r3 ^= r0; \
    r3 ^= r1; \
    r2 &= r3; \
    r4 ^= r2; \
    \
    w = r0; x = r4; y = r1; z = r3; \
  }

#define SBOX1(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r0 = ~r0; r2 = ~r2; \
    r4 =  r0; r0 &= r1; \
    r2 ^= r0; r0 |= r3; \
    r3 ^= r2; r1 ^= r0; \
    r0 ^= r4; r4 |= r1; \
    r1 ^= r3; r2 |= r0; \
    r2 &= r4; r0 ^= r1; \
    r1 &= r2; \
    r1 ^= r0; r0 &= r2; \
    r0 ^= r4; \
    \
    w = r2; x = r0; y = r3; z = r1; \
  }

#define SBOX1_INVERSE(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r4 =  r1; r1 ^= r3; \
    r3 &= r1; r4 ^= r2; \
    r3 ^= r0; r0 |= r1; \
    r2 ^= r3; r0 ^= r4; \
    r0 |= r2; r1 ^= r3; \
    r0 ^= r1; r1 |= r3; \
    r1 ^= r0; r4 = ~r4; \
    r4 ^= r1; r1 |= r0; \
    r1 ^= r0; \
    r1 |= r4; \
    r3 ^= r1; \
    \
    w = r4; x = r0; y = r3; z = r2; \
  }

#define SBOX2(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r4 =  r0; r0 &= r2; \
    r0 ^= r3; r2 ^= r1; \
    r2 ^= r0; r3 |= r4; \
    r3 ^= r1; r4 ^= r2; \
    r1 =  r3; r3 |= r4; \
    r3 ^= r0; r0 &= r1; \
    r4 ^= r0; r1 ^= r3; \
    r1 ^= r4; r4 = ~r4; \
    \
    w = r2; x = r3; y = r1; z = r4; \
  }

#define SBOX2_INVERSE(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r2 ^= r3; r3 ^= r0; \
    r4 =  r3; r3 &= r2; \
    r3 ^= r1; r1 |= r2; \
    r1 ^= r4; r4 &= r3; \
    r2 ^= r3; r4 &= r0; \
    r4 ^= r2; r2 &= r1; \
    r2 |= r0; r3 = ~r3; \
    r2 ^= r3; r0 ^= r3; \
    r0 &= r1; r3 ^= r4; \
    r3 ^= r0; \
    \
    w = r1; x = r4; y = r2; z = r3; \
  }

#define SBOX3(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r4 =  r0; r0 |= r3; \
    r3 ^= r1; r1 &= r4; \
    r4 ^= r2; r2 ^= r3; \
    r3 &= r0; r4 |= r1; \
    r3 ^= r4; r0 ^= r1; \
    r4 &= r0; r1 ^= r3; \
    r4 ^= r2; r1 |= r0; \
    r1 ^= r2; r0 ^= r3; \
    r2 =  r1; r1 |= r3; \
    r1 ^= r0; \
    \
    w = r1; x = r2; y = r3; z = r4; \
  }

#define SBOX3_INVERSE(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r4 =  r2; r2 ^= r1; \
    r0 ^= r2; r4 &= r2; \
    r4 ^= r0; r0 &= r1; \
    r1 ^= r3; r3 |= r4; \
    r2 ^= r3; r0 ^= r3; \
    r1 ^= r4; r3 &= r2; \
    r3 ^= r1; r1 ^= r0; \
    r1 |= r2; r0 ^= r3; \
    r1 ^= r4; \
    r0 ^= r1; \
    \
    w = r2; x = r1; y = r3; z = r0; \
  }

#define SBOX4(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r1 ^= r3; r3 = ~r3; \
    r2 ^= r3; r3 ^= r0; \
    r4 =  r1; r1 &= r3; \
    r1 ^= r2; r4 ^= r3; \
    r0 ^= r4; r2 &= r4; \
    r2 ^= r0; r0 &= r1; \
    r3 ^= r0; r4 |= r1; \
    r4 ^= r0; r0 |= r3; \
    r0 ^= r2; r2 &= r3; \
    r0 = ~r0; r4 ^= r2; \
    \
    w = r1; x = r4; y = r0; z = r3; \
  }

#define SBOX4_INVERSE(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r4 =  r2; r2 &= r3; \
    r2 ^= r1; r1 |= r3; \
    r1 &= r0; r4 ^= r2; \
    r4 ^= r1; r1 &= r2; \
    r0 = ~r0; r3 ^= r4; \
    r1 ^= r3; r3 &= r0; \
    r3 ^= r2; r0 ^= r1; \
    r2 &= r0; r3 ^= r0; \
    r2 ^= r4; \
    r2 |= r3; r3 ^= r0; \
    r2 ^= r1; \
    \
    w = r0; x = r3; y = r2; z = r4; \
  }

#define SBOX5(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r0 ^= r1; r1 ^= r3; \
    r3 = ~r3; r4 =  r1; \
    r1 &= r0; r2 ^= r3; \
    r1 ^= r2; r2 |= r4; \
    r4 ^= r3; r3 &= r1; \
    r3 ^= r0; r4 ^= r1; \
    r4 ^= r2; r2 ^= r0; \
    r0 &= r3; r2 = ~r2; \
    r0 ^= r4; r4 |= r3; \
    r2 ^= r4; \
    \
    w = r1; x = r3; y = r0; z = r2; \
  }

#define SBOX5_INVERSE(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r1 = ~r1; r4 =  r3; \
    r2 ^= r1; r3 |= r0; \
    r3 ^= r2; r2 |= r1; \
    r2 &= r0; r4 ^= r3; \
    r2 ^= r4; r4 |= r0; \
    r4 ^= r1; r1 &= r2; \
    r1 ^= r3; r4 ^= r2; \
    r3 &= r4; r4 ^= r1; \
    r3 ^= r4; r4 = ~r4; \
    r3 ^= r0; \
    \
    w = r1; x = r4; y = r3; z = r2; \
  }

#define SBOX6(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r2 = ~r2; r4 =  r3; \
    r3 &= r0; r0 ^= r4; \
    r3 ^= r2; r2 |= r4; \
    r1 ^= r3; r2 ^= r0; \
    r0 |= r1; r2 ^= r1; \
    r4 ^= r0; r0 |= r3; \
    r0 ^= r2; r4 ^= r3; \
    r4 ^= r0; r3 = ~r3; \
    r2 &= r4; \
    r2 ^= r3; \
    \
    w = r0; x = r1; y = r4; z = r2; \
  }

#define SBOX6_INVERSE(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r0 ^= r2; r4 =  r2; \
    r2 &= r0; r4 ^= r3; \
    r2 = ~r2; r3 ^= r1; \
    r2 ^= r3; r4 |= r0; \
    r0 ^= r2; r3 ^= r4; \
    r4 ^= r1; r1 &= r3; \
    r1 ^= r0; r0 ^= r3; \
    r0 |= r2; r3 ^= r1; \
    r4 ^= r0; \
    \
    w = r1; x = r2; y = r4; z = r3; \
  }

#define SBOX7(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r4 =  r1; r1 |= r2; \
    r1 ^= r3; r4 ^= r2; \
    r2 ^= r1; r3 |= r4; \
    r3 &= r0; r4 ^= r2; \
    r3 ^= r1; r1 |= r4; \
    r1 ^= r0; r0 |= r4; \
    r0 ^= r2; r1 ^= r4; \
    r2 ^= r1; r1 &= r0; \
    r1 ^= r4; r2 = ~r2; \
    r2 |= r0; \
    r4 ^= r2; \
    \
    w = r4; x = r3; y = r1; z = r0; \
  }

#define SBOX7_INVERSE(r0, r1, r2, r3, w, x, y, z) \
  { \
    __m512i r4; \
    \
    r4 =  r2; r2 ^= r0; \
    r0 &= r3; r4 |= r3; \
    r2 = ~r2; r3 ^= r1; \
    r1 |= r0; r0 ^= r2; \
    r2 &= r4; r3 &= r4; \
    r1 ^= r2; r2 ^= r0; \
    r0 |= r2; r4 ^= r1; \
    r0 ^= r3; r3 ^= r4; \
    r4 |= r0; r3 ^= r2; \
    r4 ^= r2; \
    \
    w = r3; x = r0; y = r1; z = r4; \
  }

/* XOR BLOCK1 into BLOCK0.  */
#define BLOCK_XOR_KEY(block0, rkey)     \
  {                                     \
    block0[0] ^= vpbroadcastd(rkey[0]); \
    block0[1] ^= vpbroadcastd(rkey[1]); \
    block0[2] ^= vpbroadcastd(rkey[2]); \
    block0[3] ^= vpbroadcastd(rkey[3]); \
  }

/* Copy BLOCK_SRC to BLOCK_DST.  */
#define BLOCK_COPY(block_dst, block_src) \
  {                                      \
    block_dst[0] = block_src[0];         \
    block_dst[1] = block_src[1];         \
    block_dst[2] = block_src[2];         \
    block_dst[3] = block_src[3];         \
  }

/* Apply SBOX number WHICH to to the block found in ARRAY0, writing
   the output to the block found in ARRAY1.  */
#define SBOX(which, array0, array1)                         \
  SBOX##which (array0[0], array0[1], array0[2], array0[3],  \
               array1[0], array1[1], array1[2], array1[3]);

/* Apply inverse SBOX number WHICH to to the block found in ARRAY0, writing
   the output to the block found in ARRAY1.  */
#define SBOX_INVERSE(which, array0, array1)                           \
  SBOX##which##_INVERSE (array0[0], array0[1], array0[2], array0[3],  \
                         array1[0], array1[1], array1[2], array1[3]);

/* Apply the linear transformation to BLOCK.  */
#define LINEAR_TRANSFORMATION(block)                    \
  {                                                     \
    block[0] = vrol (block[0], 13);                     \
    block[2] = vrol (block[2], 3);                      \
    block[1] = block[1] ^ block[0] ^ block[2];          \
    block[3] = block[3] ^ block[2] ^ vshl(block[0], 3); \
    block[1] = vrol (block[1], 1);                      \
    block[3] = vrol (block[3], 7);                      \
    block[0] = block[0] ^ block[1] ^ block[3];          \
    block[2] = block[2] ^ block[3] ^ vshl(block[1], 7); \
    block[0] = vrol (block[0], 5);                      \
    block[2] = vrol (block[2], 22);                     \
  }

/* Apply the inverse linear transformation to BLOCK.  */
#define LINEAR_TRANSFORMATION_INVERSE(block)            \
  {                                                     \
    block[2] = vror (block[2], 22);                     \
    block[0] = vror (block[0] , 5);                     \
    block[2] = block[2] ^ block[3] ^ vshl(block[1], 7); \
    block[0] = block[0] ^ block[1] ^ block[3];          \
    block[3] = vror (block[3], 7);                      \
    block[1] = vror (block[1], 1);                      \
    block[3] = block[3] ^ block[2] ^ vshl(block[0], 3); \
    block[1] = block[1] ^ block[0] ^ block[2];          \
    block[2] = vror (block[2], 3);                      \
    block[0] = vror (block[0], 13);                     \
  }

/* Apply a Serpent round to BLOCK, using the SBOX number WHICH and the
   subkeys contained in SUBKEYS.  Use BLOCK_TMP as temporary storage.
   This macro increments `round'.  */
#define ROUND(which, subkeys, block, block_tmp) \
  {                                             \
    BLOCK_XOR_KEY (block, subkeys[round]);      \
    SBOX (which, block, block_tmp);             \
    LINEAR_TRANSFORMATION (block_tmp);          \
    BLOCK_COPY (block, block_tmp);              \
  }

/* Apply the last Serpent round to BLOCK, using the SBOX number WHICH
   and the subkeys contained in SUBKEYS.  Use BLOCK_TMP as temporary
   storage.  The result will be stored in BLOCK_TMP.  This macro
   increments `round'.  */
#define ROUND_LAST(which, subkeys, block, block_tmp) \
  {                                                  \
    BLOCK_XOR_KEY (block, subkeys[round]);           \
    SBOX (which, block, block_tmp);                  \
    BLOCK_XOR_KEY (block_tmp, subkeys[round+1]);     \
  }

/* Apply an inverse Serpent round to BLOCK, using the SBOX number
   WHICH and the subkeys contained in SUBKEYS.  Use BLOCK_TMP as
   temporary storage.  This macro increments `round'.  */
#define ROUND_INVERSE(which, subkey, block, block_tmp) \
  {                                                    \
    LINEAR_TRANSFORMATION_INVERSE (block);             \
    SBOX_INVERSE (which, block, block_tmp);            \
    BLOCK_XOR_KEY (block_tmp, subkey[round]);          \
    BLOCK_COPY (block, block_tmp);                     \
  }

/* Apply the first Serpent round to BLOCK, using the SBOX number WHICH
   and the subkeys contained in SUBKEYS.  Use BLOCK_TMP as temporary
   storage.  The result will be stored in BLOCK_TMP.  This macro
   increments `round'.  */
#define ROUND_FIRST_INVERSE(which, subkeys, block, block_tmp) \
  {                                                           \
    BLOCK_XOR_KEY (block, subkeys[round]);                    \
    SBOX_INVERSE (which, block, block_tmp);                   \
    BLOCK_XOR_KEY (block_tmp, subkeys[round-1]);              \
  }

static ALWAYS_INLINE void
serpent_encrypt_internal_avx512 (const serpent_subkeys_t keys,
				 const __m512i vin[8], __m512i vout[8])
{
  __m512i b[4];
  __m512i c[4];
  __m512i b_next[4];
  __m512i c_next[4];
  int round = 0;

  b_next[0] = vin[0];
  b_next[1] = vin[1];
  b_next[2] = vin[2];
  b_next[3] = vin[3];
  c_next[0] = vin[4];
  c_next[1] = vin[5];
  c_next[2] = vin[6];
  c_next[3] = vin[7];
  transpose_4x4 (b_next[0], b_next[1], b_next[2], b_next[3], b[0], b[1], b[2]);
  transpose_4x4 (c_next[0], c_next[1], c_next[2], c_next[3], c[0], c[1], c[2]);

  b[0] = b_next[0];
  b[1] = b_next[1];
  b[2] = b_next[2];
  b[3] = b_next[3];
  c[0] = c_next[0];
  c[1] = c_next[1];
  c[2] = c_next[2];
  c[3] = c_next[3];

  while (1)
    {
      ROUND (0, keys, b, b_next); ROUND (0, keys, c, c_next); round++;
      ROUND (1, keys, b, b_next); ROUND (1, keys, c, c_next); round++;
      ROUND (2, keys, b, b_next); ROUND (2, keys, c, c_next); round++;
      ROUND (3, keys, b, b_next); ROUND (3, keys, c, c_next); round++;
      ROUND (4, keys, b, b_next); ROUND (4, keys, c, c_next); round++;
      ROUND (5, keys, b, b_next); ROUND (5, keys, c, c_next); round++;
      ROUND (6, keys, b, b_next); ROUND (6, keys, c, c_next); round++;
      if (round >= ROUNDS - 1)
	break;
      ROUND (7, keys, b, b_next); ROUND (7, keys, c, c_next); round++;
    }

  ROUND_LAST (7, keys, b, b_next); ROUND_LAST (7, keys, c, c_next);

  transpose_4x4 (b_next[0], b_next[1], b_next[2], b_next[3], b[0], b[1], b[2]);
  transpose_4x4 (c_next[0], c_next[1], c_next[2], c_next[3], c[0], c[1], c[2]);
  vout[0] = b_next[0];
  vout[1] = b_next[1];
  vout[2] = b_next[2];
  vout[3] = b_next[3];
  vout[4] = c_next[0];
  vout[5] = c_next[1];
  vout[6] = c_next[2];
  vout[7] = c_next[3];
}

static ALWAYS_INLINE void
serpent_decrypt_internal_avx512 (const serpent_subkeys_t keys,
				 const __m512i vin[8], __m512i vout[8])
{
  __m512i b[4];
  __m512i c[4];
  __m512i b_next[4];
  __m512i c_next[4];
  int round = ROUNDS;

  b_next[0] = vin[0];
  b_next[1] = vin[1];
  b_next[2] = vin[2];
  b_next[3] = vin[3];
  c_next[0] = vin[4];
  c_next[1] = vin[5];
  c_next[2] = vin[6];
  c_next[3] = vin[7];
  transpose_4x4 (b_next[0], b_next[1], b_next[2], b_next[3], b[0], b[1], b[2]);
  transpose_4x4 (c_next[0], c_next[1], c_next[2], c_next[3], c[0], c[1], c[2]);

  ROUND_FIRST_INVERSE (7, keys, b_next, b); ROUND_FIRST_INVERSE (7, keys, c_next, c);
  round -= 2;

  while (1)
    {
      ROUND_INVERSE (6, keys, b, b_next); ROUND_INVERSE (6, keys, c, c_next); round--;
      ROUND_INVERSE (5, keys, b, b_next); ROUND_INVERSE (5, keys, c, c_next); round--;
      ROUND_INVERSE (4, keys, b, b_next); ROUND_INVERSE (4, keys, c, c_next); round--;
      ROUND_INVERSE (3, keys, b, b_next); ROUND_INVERSE (3, keys, c, c_next); round--;
      ROUND_INVERSE (2, keys, b, b_next); ROUND_INVERSE (2, keys, c, c_next); round--;
      ROUND_INVERSE (1, keys, b, b_next); ROUND_INVERSE (1, keys, c, c_next); round--;
      ROUND_INVERSE (0, keys, b, b_next); ROUND_INVERSE (0, keys, c, c_next); round--;
      if (round <= 0)
	break;
      ROUND_INVERSE (7, keys, b, b_next); ROUND_INVERSE (7, keys, c, c_next); round--;
    }

  transpose_4x4 (b_next[0], b_next[1], b_next[2], b_next[3], b[0], b[1], b[2]);
  transpose_4x4 (c_next[0], c_next[1], c_next[2], c_next[3], c[0], c[1], c[2]);
  vout[0] = b_next[0];
  vout[1] = b_next[1];
  vout[2] = b_next[2];
  vout[3] = b_next[3];
  vout[4] = c_next[0];
  vout[5] = c_next[1];
  vout[6] = c_next[2];
  vout[7] = c_next[3];
}

enum crypt_mode_e
{
  ECB_ENC = 0,
  ECB_DEC,
  CBC_DEC,
  CFB_DEC,
  CTR_ENC,
  OCB_ENC,
  OCB_DEC
};

static ALWAYS_INLINE void
ctr_generate(unsigned char *ctr, __m512i vin[8])
{
  const unsigned int blocksize = 16;
  unsigned char ctr_low = ctr[15];

  if (ctr_low + 32 <= 256)
    {
      const __m512i add0123 = _mm512_set_epi64(3LL << 56, 0,
					       2LL << 56, 0,
					       1LL << 56, 0,
					       0LL << 56, 0);
      const __m512i add4444 = _mm512_set_epi64(4LL << 56, 0,
					       4LL << 56, 0,
					       4LL << 56, 0,
					       4LL << 56, 0);
      const __m512i add4567 = _mm512_add_epi32(add0123, add4444);
      const __m512i add8888 = _mm512_add_epi32(add4444, add4444);

      // Fast path without carry handling.
      __m512i vctr =
	_mm512_broadcast_i32x4(_mm_loadu_si128((const void *)ctr));

      cipher_block_add(ctr, 32, blocksize);
      vin[0] = _mm512_add_epi32(vctr, add0123);
      vin[1] = _mm512_add_epi32(vctr, add4567);
      vin[2] = _mm512_add_epi32(vin[0], add8888);
      vin[3] = _mm512_add_epi32(vin[1], add8888);
      vin[4] = _mm512_add_epi32(vin[2], add8888);
      vin[5] = _mm512_add_epi32(vin[3], add8888);
      vin[6] = _mm512_add_epi32(vin[4], add8888);
      vin[7] = _mm512_add_epi32(vin[5], add8888);
    }
  else
    {
      // Slow path.
      u32 blocks[4][blocksize / sizeof(u32)];

      cipher_block_cpy(blocks[0], ctr, blocksize);
      cipher_block_cpy(blocks[1], ctr, blocksize);
      cipher_block_cpy(blocks[2], ctr, blocksize);
      cipher_block_cpy(blocks[3], ctr, blocksize);
      cipher_block_add(ctr, 32, blocksize);
      cipher_block_add(blocks[1], 1, blocksize);
      cipher_block_add(blocks[2], 2, blocksize);
      cipher_block_add(blocks[3], 3, blocksize);
      vin[0] = _mm512_loadu_epi32 (blocks);
      cipher_block_add(blocks[0], 4, blocksize);
      cipher_block_add(blocks[1], 4, blocksize);
      cipher_block_add(blocks[2], 4, blocksize);
      cipher_block_add(blocks[3], 4, blocksize);
      vin[1] = _mm512_loadu_epi32 (blocks);
      cipher_block_add(blocks[0], 4, blocksize);
      cipher_block_add(blocks[1], 4, blocksize);
      cipher_block_add(blocks[2], 4, blocksize);
      cipher_block_add(blocks[3], 4, blocksize);
      vin[2] = _mm512_loadu_epi32 (blocks);
      cipher_block_add(blocks[0], 4, blocksize);
      cipher_block_add(blocks[1], 4, blocksize);
      cipher_block_add(blocks[2], 4, blocksize);
      cipher_block_add(blocks[3], 4, blocksize);
      vin[3] = _mm512_loadu_epi32 (blocks);
      cipher_block_add(blocks[0], 4, blocksize);
      cipher_block_add(blocks[1], 4, blocksize);
      cipher_block_add(blocks[2], 4, blocksize);
      cipher_block_add(blocks[3], 4, blocksize);
      vin[4] = _mm512_loadu_epi32 (blocks);
      cipher_block_add(blocks[0], 4, blocksize);
      cipher_block_add(blocks[1], 4, blocksize);
      cipher_block_add(blocks[2], 4, blocksize);
      cipher_block_add(blocks[3], 4, blocksize);
      vin[5] = _mm512_loadu_epi32 (blocks);
      cipher_block_add(blocks[0], 4, blocksize);
      cipher_block_add(blocks[1], 4, blocksize);
      cipher_block_add(blocks[2], 4, blocksize);
      cipher_block_add(blocks[3], 4, blocksize);
      vin[6] = _mm512_loadu_epi32 (blocks);
      cipher_block_add(blocks[0], 4, blocksize);
      cipher_block_add(blocks[1], 4, blocksize);
      cipher_block_add(blocks[2], 4, blocksize);
      cipher_block_add(blocks[3], 4, blocksize);
      vin[7] = _mm512_loadu_epi32 (blocks);

      wipememory(blocks, sizeof(blocks));
    }
}

static ALWAYS_INLINE __m512i
ocb_input(__m512i *vchecksum, __m128i *voffset, const unsigned char *input,
	  unsigned char *output, const ocb_L_uintptr_t L[4])
{
  __m128i L0 = _mm_loadu_si128((const void *)(uintptr_t)L[0]);
  __m128i L1 = _mm_loadu_si128((const void *)(uintptr_t)L[1]);
  __m128i L2 = _mm_loadu_si128((const void *)(uintptr_t)L[2]);
  __m128i L3 = _mm_loadu_si128((const void *)(uintptr_t)L[3]);
  __m512i vin = _mm512_loadu_epi32 (input);
  __m512i voffsets;

  /* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
  /* Checksum_i = Checksum_{i-1} xor P_i  */
  /* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */

  if (vchecksum)
    *vchecksum ^= _mm512_loadu_epi32 (input);

  *voffset ^= L0;
  voffsets = _mm512_castsi128_si512(*voffset);
  *voffset ^= L1;
  voffsets = _mm512_inserti32x4(voffsets, *voffset, 1);
  *voffset ^= L2;
  voffsets = _mm512_inserti32x4(voffsets, *voffset, 2);
  *voffset ^= L3;
  voffsets = _mm512_inserti32x4(voffsets, *voffset, 3);
  _mm512_storeu_epi32 (output, voffsets);

  return vin ^ voffsets;
}

static NO_INLINE void
serpent_avx512_blk32(const void *c, unsigned char *output,
		     const unsigned char *input, int mode,
		     unsigned char *iv, unsigned char *checksum,
		     const ocb_L_uintptr_t Ls[32])
{
  __m512i vin[8];
  __m512i vout[8];
  int encrypt = 1;

  asm volatile ("vpxor %%ymm0, %%ymm0, %%ymm0;\n\t"
		"vpopcntb %%zmm0, %%zmm6;\n\t" /* spec stop for old AVX512 CPUs */
		"vpxor %%ymm6, %%ymm6, %%ymm6;\n\t"
		:
		: "m"(*input), "m"(*output)
		: "xmm6", "xmm0", "memory", "cc");

  // Input handling
  switch (mode)
    {
      default:
      case CBC_DEC:
      case ECB_DEC:
	encrypt = 0;
	/* fall through */
      case ECB_ENC:
	vin[0] = _mm512_loadu_epi32 (input + 0 * 64);
	vin[1] = _mm512_loadu_epi32 (input + 1 * 64);
	vin[2] = _mm512_loadu_epi32 (input + 2 * 64);
	vin[3] = _mm512_loadu_epi32 (input + 3 * 64);
	vin[4] = _mm512_loadu_epi32 (input + 4 * 64);
	vin[5] = _mm512_loadu_epi32 (input + 5 * 64);
	vin[6] = _mm512_loadu_epi32 (input + 6 * 64);
	vin[7] = _mm512_loadu_epi32 (input + 7 * 64);
	break;

      case CFB_DEC:
      {
	__m128i viv;
	vin[0] = _mm512_maskz_loadu_epi32(_cvtu32_mask16(0xfff0),
					  input - 1 * 64 + 48)
		  ^ _mm512_maskz_loadu_epi32(_cvtu32_mask16(0x000f), iv);
	vin[1] = _mm512_loadu_epi32(input + 0 * 64 + 48);
	vin[2] = _mm512_loadu_epi32(input + 1 * 64 + 48);
	vin[3] = _mm512_loadu_epi32(input + 2 * 64 + 48);
	vin[4] = _mm512_loadu_epi32(input + 3 * 64 + 48);
	vin[5] = _mm512_loadu_epi32(input + 4 * 64 + 48);
	vin[6] = _mm512_loadu_epi32(input + 5 * 64 + 48);
	vin[7] = _mm512_loadu_epi32(input + 6 * 64 + 48);
	viv = _mm_loadu_si128((const void *)(input + 7 * 64 + 48));
	_mm_storeu_si128((void *)iv, viv);
	break;
      }

      case CTR_ENC:
	ctr_generate(iv, vin);
	break;

      case OCB_ENC:
      {
	const ocb_L_uintptr_t *L = Ls;
	__m512i vchecksum = _mm512_setzero_epi32();
	__m128i vchecksum128 = _mm_loadu_si128((const void *)checksum);
	__m128i voffset = _mm_loadu_si128((const void *)iv);
	vin[0] = ocb_input(&vchecksum, &voffset, input + 0 * 64, output + 0 * 64, L); L += 4;
	vin[1] = ocb_input(&vchecksum, &voffset, input + 1 * 64, output + 1 * 64, L); L += 4;
	vin[2] = ocb_input(&vchecksum, &voffset, input + 2 * 64, output + 2 * 64, L); L += 4;
	vin[3] = ocb_input(&vchecksum, &voffset, input + 3 * 64, output + 3 * 64, L); L += 4;
	vin[4] = ocb_input(&vchecksum, &voffset, input + 4 * 64, output + 4 * 64, L); L += 4;
	vin[5] = ocb_input(&vchecksum, &voffset, input + 5 * 64, output + 5 * 64, L); L += 4;
	vin[6] = ocb_input(&vchecksum, &voffset, input + 6 * 64, output + 6 * 64, L); L += 4;
	vin[7] = ocb_input(&vchecksum, &voffset, input + 7 * 64, output + 7 * 64, L);
	vchecksum128 ^= _mm512_extracti32x4_epi32(vchecksum, 0)
			^ _mm512_extracti32x4_epi32(vchecksum, 1)
			^ _mm512_extracti32x4_epi32(vchecksum, 2)
			^ _mm512_extracti32x4_epi32(vchecksum, 3);
	_mm_storeu_si128((void *)checksum, vchecksum128);
	_mm_storeu_si128((void *)iv, voffset);
	break;
      }

      case OCB_DEC:
      {
	const ocb_L_uintptr_t *L = Ls;
	__m128i voffset = _mm_loadu_si128((const void *)iv);
	encrypt = 0;
	vin[0] = ocb_input(NULL, &voffset, input + 0 * 64, output + 0 * 64, L); L += 4;
	vin[1] = ocb_input(NULL, &voffset, input + 1 * 64, output + 1 * 64, L); L += 4;
	vin[2] = ocb_input(NULL, &voffset, input + 2 * 64, output + 2 * 64, L); L += 4;
	vin[3] = ocb_input(NULL, &voffset, input + 3 * 64, output + 3 * 64, L); L += 4;
	vin[4] = ocb_input(NULL, &voffset, input + 4 * 64, output + 4 * 64, L); L += 4;
	vin[5] = ocb_input(NULL, &voffset, input + 5 * 64, output + 5 * 64, L); L += 4;
	vin[6] = ocb_input(NULL, &voffset, input + 6 * 64, output + 6 * 64, L); L += 4;
	vin[7] = ocb_input(NULL, &voffset, input + 7 * 64, output + 7 * 64, L);
	_mm_storeu_si128((void *)iv, voffset);
	break;
      }
    }

  if (encrypt)
    serpent_encrypt_internal_avx512(c, vin, vout);
  else
    serpent_decrypt_internal_avx512(c, vin, vout);

  switch (mode)
    {
      case CTR_ENC:
      case CFB_DEC:
	vout[0] ^= _mm512_loadu_epi32 (input + 0 * 64);
	vout[1] ^= _mm512_loadu_epi32 (input + 1 * 64);
	vout[2] ^= _mm512_loadu_epi32 (input + 2 * 64);
	vout[3] ^= _mm512_loadu_epi32 (input + 3 * 64);
	vout[4] ^= _mm512_loadu_epi32 (input + 4 * 64);
	vout[5] ^= _mm512_loadu_epi32 (input + 5 * 64);
	vout[6] ^= _mm512_loadu_epi32 (input + 6 * 64);
	vout[7] ^= _mm512_loadu_epi32 (input + 7 * 64);
	/* fall through */
      default:
      case ECB_DEC:
      case ECB_ENC:
	_mm512_storeu_epi32 (output + 0 * 64, vout[0]);
	_mm512_storeu_epi32 (output + 1 * 64, vout[1]);
	_mm512_storeu_epi32 (output + 2 * 64, vout[2]);
	_mm512_storeu_epi32 (output + 3 * 64, vout[3]);
	_mm512_storeu_epi32 (output + 4 * 64, vout[4]);
	_mm512_storeu_epi32 (output + 5 * 64, vout[5]);
	_mm512_storeu_epi32 (output + 6 * 64, vout[6]);
	_mm512_storeu_epi32 (output + 7 * 64, vout[7]);
	break;

      case CBC_DEC:
      {
	__m128i viv;
	vout[0] ^= _mm512_maskz_loadu_epi32(_cvtu32_mask16(0xfff0),
					    input - 1 * 64 + 48)
		    ^ _mm512_maskz_loadu_epi32(_cvtu32_mask16(0x000f), iv);
	vout[1] ^= _mm512_loadu_epi32(input + 0 * 64 + 48);
	vout[2] ^= _mm512_loadu_epi32(input + 1 * 64 + 48);
	vout[3] ^= _mm512_loadu_epi32(input + 2 * 64 + 48);
	vout[4] ^= _mm512_loadu_epi32(input + 3 * 64 + 48);
	vout[5] ^= _mm512_loadu_epi32(input + 4 * 64 + 48);
	vout[6] ^= _mm512_loadu_epi32(input + 5 * 64 + 48);
	vout[7] ^= _mm512_loadu_epi32(input + 6 * 64 + 48);
	viv = _mm_loadu_si128((const void *)(input + 7 * 64 + 48));
	_mm_storeu_si128((void *)iv, viv);
	_mm512_storeu_epi32 (output + 0 * 64, vout[0]);
	_mm512_storeu_epi32 (output + 1 * 64, vout[1]);
	_mm512_storeu_epi32 (output + 2 * 64, vout[2]);
	_mm512_storeu_epi32 (output + 3 * 64, vout[3]);
	_mm512_storeu_epi32 (output + 4 * 64, vout[4]);
	_mm512_storeu_epi32 (output + 5 * 64, vout[5]);
	_mm512_storeu_epi32 (output + 6 * 64, vout[6]);
	_mm512_storeu_epi32 (output + 7 * 64, vout[7]);
	break;
      }

      case OCB_ENC:
	vout[0] ^= _mm512_loadu_epi32 (output + 0 * 64);
	vout[1] ^= _mm512_loadu_epi32 (output + 1 * 64);
	vout[2] ^= _mm512_loadu_epi32 (output + 2 * 64);
	vout[3] ^= _mm512_loadu_epi32 (output + 3 * 64);
	vout[4] ^= _mm512_loadu_epi32 (output + 4 * 64);
	vout[5] ^= _mm512_loadu_epi32 (output + 5 * 64);
	vout[6] ^= _mm512_loadu_epi32 (output + 6 * 64);
	vout[7] ^= _mm512_loadu_epi32 (output + 7 * 64);
	_mm512_storeu_epi32 (output + 0 * 64, vout[0]);
	_mm512_storeu_epi32 (output + 1 * 64, vout[1]);
	_mm512_storeu_epi32 (output + 2 * 64, vout[2]);
	_mm512_storeu_epi32 (output + 3 * 64, vout[3]);
	_mm512_storeu_epi32 (output + 4 * 64, vout[4]);
	_mm512_storeu_epi32 (output + 5 * 64, vout[5]);
	_mm512_storeu_epi32 (output + 6 * 64, vout[6]);
	_mm512_storeu_epi32 (output + 7 * 64, vout[7]);
	break;

      case OCB_DEC:
      {
	__m512i vchecksum = _mm512_setzero_epi32();
	__m128i vchecksum128 = _mm_loadu_si128((const void *)checksum);
	vout[0] ^= _mm512_loadu_epi32 (output + 0 * 64);
	vout[1] ^= _mm512_loadu_epi32 (output + 1 * 64);
	vout[2] ^= _mm512_loadu_epi32 (output + 2 * 64);
	vout[3] ^= _mm512_loadu_epi32 (output + 3 * 64);
	vout[4] ^= _mm512_loadu_epi32 (output + 4 * 64);
	vout[5] ^= _mm512_loadu_epi32 (output + 5 * 64);
	vout[6] ^= _mm512_loadu_epi32 (output + 6 * 64);
	vout[7] ^= _mm512_loadu_epi32 (output + 7 * 64);
	vchecksum ^= vout[0];
	vchecksum ^= vout[1];
	vchecksum ^= vout[2];
	vchecksum ^= vout[3];
	vchecksum ^= vout[4];
	vchecksum ^= vout[5];
	vchecksum ^= vout[6];
	vchecksum ^= vout[7];
	_mm512_storeu_epi32 (output + 0 * 64, vout[0]);
	_mm512_storeu_epi32 (output + 1 * 64, vout[1]);
	_mm512_storeu_epi32 (output + 2 * 64, vout[2]);
	_mm512_storeu_epi32 (output + 3 * 64, vout[3]);
	_mm512_storeu_epi32 (output + 4 * 64, vout[4]);
	_mm512_storeu_epi32 (output + 5 * 64, vout[5]);
	_mm512_storeu_epi32 (output + 6 * 64, vout[6]);
	_mm512_storeu_epi32 (output + 7 * 64, vout[7]);
	vchecksum128 ^= _mm512_extracti32x4_epi32(vchecksum, 0)
			^ _mm512_extracti32x4_epi32(vchecksum, 1)
			^ _mm512_extracti32x4_epi32(vchecksum, 2)
			^ _mm512_extracti32x4_epi32(vchecksum, 3);
	_mm_storeu_si128((void *)checksum, vchecksum128);
	break;
      }
    }

  _mm256_zeroall();
#ifdef __x86_64__
  asm volatile (
#define CLEAR(mm) "vpxord %%" #mm ", %%" #mm ", %%" #mm ";\n\t"
		CLEAR(ymm16) CLEAR(ymm17) CLEAR(ymm18) CLEAR(ymm19)
		CLEAR(ymm20) CLEAR(ymm21) CLEAR(ymm22) CLEAR(ymm23)
		CLEAR(ymm24) CLEAR(ymm25) CLEAR(ymm26) CLEAR(ymm27)
		CLEAR(ymm28) CLEAR(ymm29) CLEAR(ymm30) CLEAR(ymm31)
#undef CLEAR
		:
		: "m"(*input), "m"(*output)
		: "xmm16", "xmm17", "xmm18", "xmm19",
		  "xmm20", "xmm21", "xmm22", "xmm23",
		  "xmm24", "xmm25", "xmm26", "xmm27",
		  "xmm28", "xmm29", "xmm30", "xmm31",
		  "memory", "cc");
#endif
}

void
_gcry_serpent_avx512_blk32(const void *ctx, unsigned char *out,
			   const unsigned char *in, int encrypt)
{
  serpent_avx512_blk32 (ctx, out, in, encrypt ? ECB_ENC : ECB_DEC,
			NULL, NULL, NULL);
}

void
_gcry_serpent_avx512_cbc_dec(const void *ctx, unsigned char *out,
			     const unsigned char *in, unsigned char *iv)
{
  serpent_avx512_blk32 (ctx, out, in, CBC_DEC, iv, NULL, NULL);
}

void
_gcry_serpent_avx512_cfb_dec(const void *ctx, unsigned char *out,
			     const unsigned char *in, unsigned char *iv)
{
  serpent_avx512_blk32 (ctx, out, in, CFB_DEC, iv, NULL, NULL);
}

void
_gcry_serpent_avx512_ctr_enc(const void *ctx, unsigned char *out,
			     const unsigned char *in, unsigned char *iv)
{
  serpent_avx512_blk32 (ctx, out, in, CTR_ENC, iv, NULL, NULL);
}

void
_gcry_serpent_avx512_ocb_crypt(const void *ctx, unsigned char *out,
			       const unsigned char *in, unsigned char *offset,
			       unsigned char *checksum,
			       const ocb_L_uintptr_t Ls[32], int encrypt)
{
  serpent_avx512_blk32 (ctx, out, in, encrypt ? OCB_ENC : OCB_DEC, offset,
			checksum, Ls);
}

#endif /*defined(USE_SERPENT) && defined(ENABLE_AVX512_SUPPORT)*/
#endif /*__x86_64 || __i386*/
