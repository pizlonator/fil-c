/* sm4-ppc.c  -  PowerPC implementation of SM4 cipher
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

#if defined(ENABLE_PPC_CRYPTO_SUPPORT) && \
    defined(HAVE_COMPATIBLE_CC_PPC_ALTIVEC) && \
    defined(HAVE_GCC_INLINE_ASM_PPC_ALTIVEC) && \
    !defined(WORDS_BIGENDIAN) && (__GNUC__ >= 4)

#include <altivec.h>
#include "bufhelp.h"

typedef vector unsigned char vector16x_u8;
typedef vector unsigned int vector4x_u32;
typedef vector unsigned long long vector2x_u64;

#ifdef HAVE_GCC_ATTRIBUTE_OPTIMIZE
# define FUNC_ATTR_OPT __attribute__((optimize("-O2")))
#else
# define FUNC_ATTR_OPT
#endif

#if defined(__clang__) && defined(HAVE_CLANG_ATTRIBUTE_PPC_TARGET)
# define FUNC_ATTR_TARGET_P8 __attribute__((target("arch=pwr8")))
# define FUNC_ATTR_TARGET_P9 __attribute__((target("arch=pwr9")))
# define HAVE_FUNC_ATTR_TARGET 1
#elif defined(HAVE_GCC_ATTRIBUTE_PPC_TARGET)
# define FUNC_ATTR_TARGET_P8 __attribute__((target("cpu=power8")))
# define FUNC_ATTR_TARGET_P9 __attribute__((target("cpu=power9")))
# define HAVE_FUNC_ATTR_TARGET 1
#else
# define FUNC_ATTR_TARGET_P8
# define FUNC_ATTR_TARGET_P9
# undef HAVE_FUNC_ATTR_TARGET
#endif

#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#define NO_INSTRUMENT_FUNCTION __attribute__((no_instrument_function))

#define ASM_FUNC_ATTR          NO_INSTRUMENT_FUNCTION
#define ASM_FUNC_ATTR_INLINE   ASM_FUNC_ATTR ALWAYS_INLINE
#define ASM_FUNC_ATTR_NOINLINE ASM_FUNC_ATTR NO_INLINE

#ifdef __clang__
/* clang has mismatching prototype for vec_sbox_be. */
static ASM_FUNC_ATTR_INLINE vector16x_u8
asm_sbox_be(vector16x_u8 b)
{
  vector16x_u8 o;
  __asm__ ("vsbox %0, %1\n\t" : "=v" (o) : "v" (b));
  return o;
}
#undef vec_sbox_be
#define vec_sbox_be asm_sbox_be
#endif /* __clang__ */

#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
	t2 = (vector4x_u32)vec_mergel((vector4x_u32)x0, (vector4x_u32)x1); \
	x0 = (vector4x_u32)vec_mergeh((vector4x_u32)x0, (vector4x_u32)x1); \
	\
	t1 = (vector4x_u32)vec_mergeh((vector4x_u32)x2, (vector4x_u32)x3); \
	x2 = (vector4x_u32)vec_mergel((vector4x_u32)x2, (vector4x_u32)x3); \
	\
	x1 = (vector4x_u32)vec_mergel((vector2x_u64)x0, (vector2x_u64)t1); \
	x0 = (vector4x_u32)vec_mergeh((vector2x_u64)x0, (vector2x_u64)t1); \
	\
	x3 = (vector4x_u32)vec_mergel((vector2x_u64)t2, (vector2x_u64)x2); \
	x2 = (vector4x_u32)vec_mergeh((vector2x_u64)t2, (vector2x_u64)x2);

#define filter_8bit(x, lo_t, hi_t, mask4bit, tmp0) ({ \
	tmp0 = x & mask4bit; \
	x = (vector4x_u32)((vector16x_u8)x >> 4); \
	\
	tmp0 = (vector4x_u32)vec_perm((vector16x_u8)lo_t, (vector16x_u8)lo_t, \
				      (vector16x_u8)tmp0); \
	x = (vector4x_u32)vec_perm((vector16x_u8)hi_t, (vector16x_u8)hi_t, \
				   (vector16x_u8)x); \
	x = x ^ tmp0; \
      })

#define GET_RKEY(round) vec_splat(r4keys, round)

#define ROUND4(round, s0, s1, s2, s3) ({ \
	vector4x_u32 rkey = GET_RKEY(round); \
	vector4x_u32 rx0 = rkey ^ s1 ^ s2 ^ s3; \
	filter_8bit(rx0, pre_tf_lo_s, pre_tf_hi_s, mask_0f, tmp0); \
	rx0 = (vector4x_u32)vec_sbox_be((vector16x_u8)rx0); \
	filter_8bit(rx0, post_tf_lo_s, post_tf_hi_s, mask_0f, tmp0); \
	s0 ^= rx0 ^ vec_rl(rx0, rotate2) ^ vec_rl(rx0, rotate10) ^ \
		    vec_rl(rx0, rotate18) ^ vec_rl(rx0, rotate24); \
      })

#define ROUND8(round, s0, s1, s2, s3, r0, r1, r2, r3) ({ \
	vector4x_u32 rkey = GET_RKEY(round); \
	vector4x_u32 rx0 = rkey ^ s1 ^ s2 ^ s3; \
	vector4x_u32 rx1 = rkey ^ r1 ^ r2 ^ r3; \
	filter_8bit(rx0, pre_tf_lo_s, pre_tf_hi_s, mask_0f, tmp0); \
	filter_8bit(rx1, pre_tf_lo_s, pre_tf_hi_s, mask_0f, tmp0); \
	rx0 = (vector4x_u32)vec_sbox_be((vector16x_u8)rx0); \
	rx1 = (vector4x_u32)vec_sbox_be((vector16x_u8)rx1); \
	filter_8bit(rx0, post_tf_lo_s, post_tf_hi_s, mask_0f, tmp0); \
	filter_8bit(rx1, post_tf_lo_s, post_tf_hi_s, mask_0f, tmp0); \
	s0 ^= rx0 ^ vec_rl(rx0, rotate2) ^ vec_rl(rx0, rotate10) ^ \
		    vec_rl(rx0, rotate18) ^ vec_rl(rx0, rotate24); \
	r0 ^= rx1 ^ vec_rl(rx1, rotate2) ^ vec_rl(rx1, rotate10) ^ \
		    vec_rl(rx1, rotate18) ^ vec_rl(rx1, rotate24); \
      })

static const vector4x_u32 mask_0f =
  { 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f };
static const vector2x_u64 pre_tf_lo_s =
  { 0x9096E3E575730600ULL, 0xC6C0B5B323255056ULL };
static const vector2x_u64 pre_tf_hi_s =
  { 0xE341AA08EA48A301ULL, 0xF153B81AF85AB113ULL };
static const vector2x_u64 post_tf_lo_s =
  { 0x6F53C6FA95A93C00ULL, 0xD9E5704C231F8AB6ULL };
static const vector2x_u64 post_tf_hi_s =
  { 0x9A4635E9479BE834ULL, 0x25F98A56F824578BULL };
static const vector4x_u32 rotate2 = { 2, 2, 2, 2 };
static const vector4x_u32 rotate10 = { 10, 10, 10, 10 };
static const vector4x_u32 rotate18 = { 18, 18, 18, 18 };
static const vector4x_u32 rotate24 = { 24, 24, 24, 24 };

static ASM_FUNC_ATTR_INLINE void
sm4_ppc_crypt_blk16(u32 *rk, byte *out, const byte *in)
{
  vector4x_u32 ra0, ra1, ra2, ra3;
  vector4x_u32 rb0, rb1, rb2, rb3;
  vector4x_u32 rc0, rc1, rc2, rc3;
  vector4x_u32 rd0, rd1, rd2, rd3;
  vector4x_u32 tmp0, tmp1;
  u32 *rk_end;

  ra0 = vec_revb((vector4x_u32)vec_xl(0, in + 0 * 16));
  ra1 = vec_revb((vector4x_u32)vec_xl(0, in + 1 * 16));
  ra2 = vec_revb((vector4x_u32)vec_xl(0, in + 2 * 16));
  ra3 = vec_revb((vector4x_u32)vec_xl(0, in + 3 * 16));
  rb0 = vec_revb((vector4x_u32)vec_xl(0, in + 4 * 16));
  rb1 = vec_revb((vector4x_u32)vec_xl(0, in + 5 * 16));
  rb2 = vec_revb((vector4x_u32)vec_xl(0, in + 6 * 16));
  rb3 = vec_revb((vector4x_u32)vec_xl(0, in + 7 * 16));
  in += 8 * 16;
  rc0 = vec_revb((vector4x_u32)vec_xl(0, in + 0 * 16));
  rc1 = vec_revb((vector4x_u32)vec_xl(0, in + 1 * 16));
  rc2 = vec_revb((vector4x_u32)vec_xl(0, in + 2 * 16));
  rc3 = vec_revb((vector4x_u32)vec_xl(0, in + 3 * 16));
  rd0 = vec_revb((vector4x_u32)vec_xl(0, in + 4 * 16));
  rd1 = vec_revb((vector4x_u32)vec_xl(0, in + 5 * 16));
  rd2 = vec_revb((vector4x_u32)vec_xl(0, in + 6 * 16));
  rd3 = vec_revb((vector4x_u32)vec_xl(0, in + 7 * 16));

  transpose_4x4(ra0, ra1, ra2, ra3, tmp0, tmp1);
  transpose_4x4(rb0, rb1, rb2, rb3, tmp0, tmp1);
  transpose_4x4(rc0, rc1, rc2, rc3, tmp0, tmp1);
  transpose_4x4(rd0, rd1, rd2, rd3, tmp0, tmp1);

  for (rk_end = rk + 32; rk < rk_end; rk += 4)
    {
      vector4x_u32 r4keys = vec_xl(0, rk);
      ROUND8(0, ra0, ra1, ra2, ra3, rb0, rb1, rb2, rb3);
      ROUND8(0, rc0, rc1, rc2, rc3, rd0, rd1, rd2, rd3);
      ROUND8(1, ra1, ra2, ra3, ra0, rb1, rb2, rb3, rb0);
      ROUND8(1, rc1, rc2, rc3, rc0, rd1, rd2, rd3, rd0);
      ROUND8(2, ra2, ra3, ra0, ra1, rb2, rb3, rb0, rb1);
      ROUND8(2, rc2, rc3, rc0, rc1, rd2, rd3, rd0, rd1);
      ROUND8(3, ra3, ra0, ra1, ra2, rb3, rb0, rb1, rb2);
      ROUND8(3, rc3, rc0, rc1, rc2, rd3, rd0, rd1, rd2);
    }

  transpose_4x4(ra3, ra2, ra1, ra0, tmp0, tmp1);
  transpose_4x4(rb3, rb2, rb1, rb0, tmp0, tmp1);
  transpose_4x4(rc3, rc2, rc1, rc0, tmp0, tmp1);
  transpose_4x4(rd3, rd2, rd1, rd0, tmp0, tmp1);

  vec_xst((vector16x_u8)vec_revb(ra3), 0, out + 0 * 16);
  vec_xst((vector16x_u8)vec_revb(ra2), 0, out + 1 * 16);
  vec_xst((vector16x_u8)vec_revb(ra1), 0, out + 2 * 16);
  vec_xst((vector16x_u8)vec_revb(ra0), 0, out + 3 * 16);
  vec_xst((vector16x_u8)vec_revb(rb3), 0, out + 4 * 16);
  vec_xst((vector16x_u8)vec_revb(rb2), 0, out + 5 * 16);
  vec_xst((vector16x_u8)vec_revb(rb1), 0, out + 6 * 16);
  vec_xst((vector16x_u8)vec_revb(rb0), 0, out + 7 * 16);
  out += 8 * 16;
  vec_xst((vector16x_u8)vec_revb(rc3), 0, out + 0 * 16);
  vec_xst((vector16x_u8)vec_revb(rc2), 0, out + 1 * 16);
  vec_xst((vector16x_u8)vec_revb(rc1), 0, out + 2 * 16);
  vec_xst((vector16x_u8)vec_revb(rc0), 0, out + 3 * 16);
  vec_xst((vector16x_u8)vec_revb(rd3), 0, out + 4 * 16);
  vec_xst((vector16x_u8)vec_revb(rd2), 0, out + 5 * 16);
  vec_xst((vector16x_u8)vec_revb(rd1), 0, out + 6 * 16);
  vec_xst((vector16x_u8)vec_revb(rd0), 0, out + 7 * 16);
}

static ASM_FUNC_ATTR_INLINE void
sm4_ppc_crypt_blk8(u32 *rk, byte *out, const byte *in)
{
  vector4x_u32 ra0, ra1, ra2, ra3;
  vector4x_u32 rb0, rb1, rb2, rb3;
  vector4x_u32 tmp0, tmp1;
  u32 *rk_end;

  ra0 = vec_revb((vector4x_u32)vec_xl(0, in + 0 * 16));
  ra1 = vec_revb((vector4x_u32)vec_xl(0, in + 1 * 16));
  ra2 = vec_revb((vector4x_u32)vec_xl(0, in + 2 * 16));
  ra3 = vec_revb((vector4x_u32)vec_xl(0, in + 3 * 16));
  rb0 = vec_revb((vector4x_u32)vec_xl(0, in + 4 * 16));
  rb1 = vec_revb((vector4x_u32)vec_xl(0, in + 5 * 16));
  rb2 = vec_revb((vector4x_u32)vec_xl(0, in + 6 * 16));
  rb3 = vec_revb((vector4x_u32)vec_xl(0, in + 7 * 16));

  transpose_4x4(ra0, ra1, ra2, ra3, tmp0, tmp1);
  transpose_4x4(rb0, rb1, rb2, rb3, tmp0, tmp1);

  for (rk_end = rk + 32; rk < rk_end; rk += 4)
    {
      vector4x_u32 r4keys = vec_xl(0, rk);
      ROUND8(0, ra0, ra1, ra2, ra3, rb0, rb1, rb2, rb3);
      ROUND8(1, ra1, ra2, ra3, ra0, rb1, rb2, rb3, rb0);
      ROUND8(2, ra2, ra3, ra0, ra1, rb2, rb3, rb0, rb1);
      ROUND8(3, ra3, ra0, ra1, ra2, rb3, rb0, rb1, rb2);
    }

  transpose_4x4(ra3, ra2, ra1, ra0, tmp0, tmp1);
  transpose_4x4(rb3, rb2, rb1, rb0, tmp0, tmp1);

  vec_xst((vector16x_u8)vec_revb(ra3), 0, out + 0 * 16);
  vec_xst((vector16x_u8)vec_revb(ra2), 0, out + 1 * 16);
  vec_xst((vector16x_u8)vec_revb(ra1), 0, out + 2 * 16);
  vec_xst((vector16x_u8)vec_revb(ra0), 0, out + 3 * 16);
  vec_xst((vector16x_u8)vec_revb(rb3), 0, out + 4 * 16);
  vec_xst((vector16x_u8)vec_revb(rb2), 0, out + 5 * 16);
  vec_xst((vector16x_u8)vec_revb(rb1), 0, out + 6 * 16);
  vec_xst((vector16x_u8)vec_revb(rb0), 0, out + 7 * 16);
}

static ASM_FUNC_ATTR_INLINE void
sm4_ppc_crypt_blk1_4(u32 *rk, byte *out, const byte *in, size_t nblks)
{
  vector4x_u32 ra0, ra1, ra2, ra3;
  vector4x_u32 tmp0, tmp1;
  u32 *rk_end;

  ra0 = vec_revb((vector4x_u32)vec_xl(0, in + 0 * 16));
  ra1 = ra0;
  ra2 = ra0;
  ra3 = ra0;
  if (LIKELY(nblks > 1))
    ra1 = vec_revb((vector4x_u32)vec_xl(0, in + 1 * 16));
  if (LIKELY(nblks > 2))
    ra2 = vec_revb((vector4x_u32)vec_xl(0, in + 2 * 16));
  if (LIKELY(nblks > 3))
    ra3 = vec_revb((vector4x_u32)vec_xl(0, in + 3 * 16));

  transpose_4x4(ra0, ra1, ra2, ra3, tmp0, tmp1);

  for (rk_end = rk + 32; rk < rk_end; rk += 4)
    {
      vector4x_u32 r4keys = vec_xl(0, rk);
      ROUND4(0, ra0, ra1, ra2, ra3);
      ROUND4(1, ra1, ra2, ra3, ra0);
      ROUND4(2, ra2, ra3, ra0, ra1);
      ROUND4(3, ra3, ra0, ra1, ra2);
    }

  transpose_4x4(ra3, ra2, ra1, ra0, tmp0, tmp1);

  vec_xst((vector16x_u8)vec_revb(ra3), 0, out + 0 * 16);
  if (LIKELY(nblks > 1))
    vec_xst((vector16x_u8)vec_revb(ra2), 0, out + 1 * 16);
  if (LIKELY(nblks > 2))
    vec_xst((vector16x_u8)vec_revb(ra1), 0, out + 2 * 16);
  if (LIKELY(nblks > 3))
    vec_xst((vector16x_u8)vec_revb(ra0), 0, out + 3 * 16);
}

static ASM_FUNC_ATTR_INLINE void
sm4_ppc_crypt_blk1_16(u32 *rk, byte *out, const byte *in, size_t nblks)
{
  if (nblks >= 16)
    {
      sm4_ppc_crypt_blk16(rk, out, in);
      return;
    }

  while (nblks >= 8)
    {
      sm4_ppc_crypt_blk8(rk, out, in);
      in += 8 * 16;
      out += 8 * 16;
      nblks -= 8;
    }

  while (nblks)
    {
      size_t currblks = nblks > 4 ? 4 : nblks;
      sm4_ppc_crypt_blk1_4(rk, out, in, currblks);
      in += currblks * 16;
      out += currblks * 16;
      nblks -= currblks;
    }
}

ASM_FUNC_ATTR_NOINLINE FUNC_ATTR_TARGET_P8 void
_gcry_sm4_ppc8le_crypt_blk1_16(u32 *rk, byte *out, const byte *in,
			       size_t nblks)
{
  sm4_ppc_crypt_blk1_16(rk, out, in, nblks);
}

ASM_FUNC_ATTR_NOINLINE FUNC_ATTR_TARGET_P9 void
_gcry_sm4_ppc9le_crypt_blk1_16(u32 *rk, byte *out, const byte *in,
			       size_t nblks)
{
#ifdef HAVE_FUNC_ATTR_TARGET
  /* Inline for POWER9 target optimization. */
  sm4_ppc_crypt_blk1_16(rk, out, in, nblks);
#else
  /* Target selecting not working, just call the other noinline function. */
  _gcry_sm4_ppc8le_crypt_blk1_16(rk, out, in, nblks);
#endif
}

#endif /* ENABLE_PPC_CRYPTO_SUPPORT */
