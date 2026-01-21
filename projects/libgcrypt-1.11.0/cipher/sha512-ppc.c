/* sha512-ppc.c - PowerPC vcrypto implementation of SHA-512 transform
 * Copyright (C) 2019,2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
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
    defined(USE_SHA512) && \
    __GNUC__ >= 4

#include <altivec.h>
#include "bufhelp.h"


typedef vector unsigned char vector16x_u8;
typedef vector unsigned long long vector2x_u64;


#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#define NO_INSTRUMENT_FUNCTION __attribute__((no_instrument_function))

#define ASM_FUNC_ATTR          NO_INSTRUMENT_FUNCTION
#define ASM_FUNC_ATTR_INLINE   ASM_FUNC_ATTR ALWAYS_INLINE
#define ASM_FUNC_ATTR_NOINLINE ASM_FUNC_ATTR NO_INLINE

#ifdef HAVE_GCC_ATTRIBUTE_OPTIMIZE
# define FUNC_ATTR_OPT_O2 __attribute__((optimize("-O2")))
#else
# define FUNC_ATTR_OPT_O2
#endif


#if defined(__clang__) && defined(HAVE_CLANG_ATTRIBUTE_PPC_TARGET)
# define FUNC_ATTR_TARGET_P8 __attribute__((target("arch=pwr8")))
# define FUNC_ATTR_TARGET_P9 __attribute__((target("arch=pwr9")))
#elif defined(HAVE_GCC_ATTRIBUTE_PPC_TARGET)
# define FUNC_ATTR_TARGET_P8 __attribute__((target("cpu=power8")))
# define FUNC_ATTR_TARGET_P9 __attribute__((target("cpu=power9")))
#else
# define FUNC_ATTR_TARGET_P8
# define FUNC_ATTR_TARGET_P9
#endif


static const vector2x_u64 K[80] =
  {
    { U64_C(0x428a2f98d728ae22), U64_C(0x7137449123ef65cd) },
    { U64_C(0xb5c0fbcfec4d3b2f), U64_C(0xe9b5dba58189dbbc) },
    { U64_C(0x3956c25bf348b538), U64_C(0x59f111f1b605d019) },
    { U64_C(0x923f82a4af194f9b), U64_C(0xab1c5ed5da6d8118) },
    { U64_C(0xd807aa98a3030242), U64_C(0x12835b0145706fbe) },
    { U64_C(0x243185be4ee4b28c), U64_C(0x550c7dc3d5ffb4e2) },
    { U64_C(0x72be5d74f27b896f), U64_C(0x80deb1fe3b1696b1) },
    { U64_C(0x9bdc06a725c71235), U64_C(0xc19bf174cf692694) },
    { U64_C(0xe49b69c19ef14ad2), U64_C(0xefbe4786384f25e3) },
    { U64_C(0x0fc19dc68b8cd5b5), U64_C(0x240ca1cc77ac9c65) },
    { U64_C(0x2de92c6f592b0275), U64_C(0x4a7484aa6ea6e483) },
    { U64_C(0x5cb0a9dcbd41fbd4), U64_C(0x76f988da831153b5) },
    { U64_C(0x983e5152ee66dfab), U64_C(0xa831c66d2db43210) },
    { U64_C(0xb00327c898fb213f), U64_C(0xbf597fc7beef0ee4) },
    { U64_C(0xc6e00bf33da88fc2), U64_C(0xd5a79147930aa725) },
    { U64_C(0x06ca6351e003826f), U64_C(0x142929670a0e6e70) },
    { U64_C(0x27b70a8546d22ffc), U64_C(0x2e1b21385c26c926) },
    { U64_C(0x4d2c6dfc5ac42aed), U64_C(0x53380d139d95b3df) },
    { U64_C(0x650a73548baf63de), U64_C(0x766a0abb3c77b2a8) },
    { U64_C(0x81c2c92e47edaee6), U64_C(0x92722c851482353b) },
    { U64_C(0xa2bfe8a14cf10364), U64_C(0xa81a664bbc423001) },
    { U64_C(0xc24b8b70d0f89791), U64_C(0xc76c51a30654be30) },
    { U64_C(0xd192e819d6ef5218), U64_C(0xd69906245565a910) },
    { U64_C(0xf40e35855771202a), U64_C(0x106aa07032bbd1b8) },
    { U64_C(0x19a4c116b8d2d0c8), U64_C(0x1e376c085141ab53) },
    { U64_C(0x2748774cdf8eeb99), U64_C(0x34b0bcb5e19b48a8) },
    { U64_C(0x391c0cb3c5c95a63), U64_C(0x4ed8aa4ae3418acb) },
    { U64_C(0x5b9cca4f7763e373), U64_C(0x682e6ff3d6b2b8a3) },
    { U64_C(0x748f82ee5defb2fc), U64_C(0x78a5636f43172f60) },
    { U64_C(0x84c87814a1f0ab72), U64_C(0x8cc702081a6439ec) },
    { U64_C(0x90befffa23631e28), U64_C(0xa4506cebde82bde9) },
    { U64_C(0xbef9a3f7b2c67915), U64_C(0xc67178f2e372532b) },
    { U64_C(0xca273eceea26619c), U64_C(0xd186b8c721c0c207) },
    { U64_C(0xeada7dd6cde0eb1e), U64_C(0xf57d4f7fee6ed178) },
    { U64_C(0x06f067aa72176fba), U64_C(0x0a637dc5a2c898a6) },
    { U64_C(0x113f9804bef90dae), U64_C(0x1b710b35131c471b) },
    { U64_C(0x28db77f523047d84), U64_C(0x32caab7b40c72493) },
    { U64_C(0x3c9ebe0a15c9bebc), U64_C(0x431d67c49c100d4c) },
    { U64_C(0x4cc5d4becb3e42b6), U64_C(0x597f299cfc657e2a) },
    { U64_C(0x5fcb6fab3ad6faec), U64_C(0x6c44198c4a475817) }
  };


static ASM_FUNC_ATTR_INLINE vector2x_u64
vec_rol_elems(vector2x_u64 v, unsigned int idx)
{
#ifndef WORDS_BIGENDIAN
  return vec_sld (v, v, (16 - (8 * idx)) & 15);
#else
  return vec_sld (v, v, (8 * idx) & 15);
#endif
}


static ASM_FUNC_ATTR_INLINE vector2x_u64
vec_merge_idx0_elems(vector2x_u64 v0, vector2x_u64 v1)
{
  return vec_mergeh (v0, v1);
}


static ASM_FUNC_ATTR_INLINE vector2x_u64
vec_vshasigma_u64(vector2x_u64 v, unsigned int a, unsigned int b)
{
  __asm__ ("vshasigmad %0,%1,%2,%3"
	   : "=v" (v)
	   : "v" (v), "g" (a), "g" (b)
	   : "memory");
  return v;
}


static ASM_FUNC_ATTR_INLINE vector2x_u64
vec_add_u64(vector2x_u64 v, vector2x_u64 w)
{
  __asm__ ("vaddudm %0,%1,%2"
	   : "=v" (v)
	   : "v" (v), "v" (w)
	   : "memory");
  return v;
}


static ASM_FUNC_ATTR_INLINE vector2x_u64
vec_u64_load(unsigned long offset, const void *ptr)
{
  vector2x_u64 vecu64;
#if __GNUC__ >= 4
  if (__builtin_constant_p (offset) && offset == 0)
    __asm__ ("lxvd2x %x0,0,%1\n\t"
	     : "=wa" (vecu64)
	     : "r" ((uintptr_t)ptr)
	     : "memory");
  else
#endif
    __asm__ ("lxvd2x %x0,%1,%2\n\t"
	     : "=wa" (vecu64)
	     : "r" (offset), "r" ((uintptr_t)ptr)
	     : "memory", "r0");
#ifndef WORDS_BIGENDIAN
  __asm__ ("xxswapd %x0, %x1"
	   : "=wa" (vecu64)
	   : "wa" (vecu64));
#endif
  return vecu64;
}


static ASM_FUNC_ATTR_INLINE void
vec_u64_store(vector2x_u64 vecu64, unsigned long offset, void *ptr)
{
#ifndef WORDS_BIGENDIAN
  __asm__ ("xxswapd %x0, %x1"
	   : "=wa" (vecu64)
	   : "wa" (vecu64));
#endif
#if __GNUC__ >= 4
  if (__builtin_constant_p (offset) && offset == 0)
    __asm__ ("stxvd2x %x0,0,%1\n\t"
	     :
	     : "wa" (vecu64), "r" ((uintptr_t)ptr)
	     : "memory");
  else
#endif
    __asm__ ("stxvd2x %x0,%1,%2\n\t"
	     :
	     : "wa" (vecu64), "r" (offset), "r" ((uintptr_t)ptr)
	     : "memory", "r0");
}


static ASM_FUNC_ATTR_INLINE vector2x_u64
vec_u64_load_be(unsigned long offset, const void *ptr)
{
  vector2x_u64 vecu64;
#if __GNUC__ >= 4
  if (__builtin_constant_p (offset) && offset == 0)
    __asm__ volatile ("lxvd2x %x0,0,%1\n\t"
		      : "=wa" (vecu64)
		      : "r" ((uintptr_t)ptr)
		      : "memory");
  else
#endif
    __asm__ volatile ("lxvd2x %x0,%1,%2\n\t"
		      : "=wa" (vecu64)
		      : "r" (offset), "r" ((uintptr_t)ptr)
		      : "memory", "r0");
#ifndef WORDS_BIGENDIAN
  return (vector2x_u64)vec_reve((vector16x_u8)vecu64);
#else
  return vecu64;
#endif
}


/* SHA2 round in vector registers */
#define R(a,b,c,d,e,f,g,h,ki,w) do                            \
    {                                                         \
      t1 = vec_add_u64((h), (w));                             \
      t2 = Cho((e),(f),(g));                                  \
      t1 = vec_add_u64(t1, GETK(ki));                         \
      t1 = vec_add_u64(t1, t2);                               \
      t1 = Sum1add(t1, e);                                    \
      t2 = Maj((a),(b),(c));                                  \
      t2 = Sum0add(t2, a);                                    \
      h  = vec_add_u64(t1, t2);                               \
      d += t1;                                                \
    } while (0)

#define GETK(kidx) \
    ({ \
      if (((kidx) % 2) == 0) \
	{ \
	  ktmp = *(kptr++); \
	  if ((kidx) < 79) \
	    asm volatile("" : "+r" (kptr) :: "memory"); \
	} \
      else \
	{ \
	  ktmp = vec_mergel(ktmp, ktmp); \
	} \
      ktmp; \
    })

#define Cho(b, c, d)  (vec_sel(d, c, b))

#define Maj(c, d, b)  (vec_sel(c, b, c ^ d))

#define Sum0(x)       (vec_vshasigma_u64(x, 1, 0))

#define Sum1(x)       (vec_vshasigma_u64(x, 1, 15))

#define S0(x)         (vec_vshasigma_u64(x, 0, 0))

#define S1(x)         (vec_vshasigma_u64(x, 0, 15))

#define Xadd(X, d, x) vec_add_u64(d, X(x))

#define Sum0add(d, x) Xadd(Sum0, d, x)

#define Sum1add(d, x) Xadd(Sum1, d, x)

#define S0add(d, x)   Xadd(S0, d, x)

#define S1add(d, x)   Xadd(S1, d, x)

#define I(i) \
    ({ \
      if (((i) % 2) == 0) \
	{ \
	  w[i] = vec_u64_load_be(0, data); \
	  data += 2 * 8; \
	  if ((i) / 2 < 7) \
	    asm volatile("" : "+r"(data) :: "memory"); \
	} \
      else \
	{ \
	  w[i] = vec_mergel(w[(i) - 1], w[(i) - 1]); \
	} \
    })

#define WN(i) ({ w[(i)&0x0f] += w[((i)-7) &0x0f];  \
		 w[(i)&0x0f] = S0add(w[(i)&0x0f], w[((i)-15)&0x0f]); \
		 w[(i)&0x0f] = S1add(w[(i)&0x0f], w[((i)-2) &0x0f]); })

#define W(i) ({ vector2x_u64 r = w[(i)&0x0f]; WN(i); r; })

#define L(i) w[(i)&0x0f]

#define I2(i) \
    ({ \
      if (((i) % 2) == 0) \
	{ \
	  w[i] = vec_u64_load_be(0, data); \
	} \
      else \
	{ \
	  vector2x_u64 it1 = vec_u64_load_be(128, data); \
	  vector2x_u64 it2 = vec_mergeh(w[(i) - 1], it1); \
	  w[i] = vec_mergel(w[(i) - 1], it1); \
	  w[(i) - 1] = it2; \
	  if ((i) < 15) \
	    { \
	      data += 2 * 8; \
	      asm volatile("" : "+r"(data) :: "memory"); \
	    } \
	  else \
	    { \
	      data += 2 * 8 + 128; \
	      asm volatile("" : "+r"(data) :: "memory"); \
	    } \
	} \
    })

#define W2(i) \
    ({ \
      vector2x_u64 wt1 = w[(i)&0x0f]; \
      WN(i); \
      w2[(i) / 2] = (((i) % 2) == 0) ? wt1 : vec_mergel(w2[(i) / 2], wt1); \
      wt1; \
    })

#define L2(i) \
    ({ \
      vector2x_u64 lt1 = w[(i)&0x0f]; \
      w2[(i) / 2] = (((i) % 2) == 0) ? lt1 : vec_mergel(w2[(i) / 2], lt1); \
      lt1; \
    })

#define WL(i) \
    ({ \
      vector2x_u64 wlt1 = w2[(i) / 2]; \
      if (((i) % 2) == 0 && (i) < 79) \
	w2[(i) / 2] = vec_mergel(wlt1, wlt1); \
      wlt1; \
    })

static ASM_FUNC_ATTR_INLINE FUNC_ATTR_OPT_O2 unsigned int
sha512_transform_ppc(u64 state[8], const unsigned char *data, size_t nblks)
{
  vector2x_u64 h0, h1, h2, h3, h4, h5, h6, h7;
  vector2x_u64 a, b, c, d, e, f, g, h, t1, t2;
  vector2x_u64 w[16];
  vector2x_u64 w2[80 / 2];

  h0 = vec_u64_load (8 * 0, (unsigned long long *)state);
  h1 = vec_rol_elems (h0, 1);
  h2 = vec_u64_load (8 * 2, (unsigned long long *)state);
  h3 = vec_rol_elems (h2, 1);
  h4 = vec_u64_load (8 * 4, (unsigned long long *)state);
  h5 = vec_rol_elems (h4, 1);
  h6 = vec_u64_load (8 * 6, (unsigned long long *)state);
  h7 = vec_rol_elems (h6, 1);

  while (nblks >= 2)
    {
      const vector2x_u64 *kptr = K;
      vector2x_u64 ktmp;

      a = h0;
      b = h1;
      c = h2;
      d = h3;
      e = h4;
      f = h5;
      g = h6;
      h = h7;

      I2(0); I2(1); I2(2); I2(3);
      I2(4); I2(5); I2(6); I2(7);
      I2(8); I2(9); I2(10); I2(11);
      I2(12); I2(13); I2(14); I2(15);

      R(a, b, c, d, e, f, g, h, 0, W2(0));
      R(h, a, b, c, d, e, f, g, 1, W2(1));
      R(g, h, a, b, c, d, e, f, 2, W2(2));
      R(f, g, h, a, b, c, d, e, 3, W2(3));
      R(e, f, g, h, a, b, c, d, 4, W2(4));
      R(d, e, f, g, h, a, b, c, 5, W2(5));
      R(c, d, e, f, g, h, a, b, 6, W2(6));
      R(b, c, d, e, f, g, h, a, 7, W2(7));

      R(a, b, c, d, e, f, g, h, 8, W2(8));
      R(h, a, b, c, d, e, f, g, 9, W2(9));
      R(g, h, a, b, c, d, e, f, 10, W2(10));
      R(f, g, h, a, b, c, d, e, 11, W2(11));
      R(e, f, g, h, a, b, c, d, 12, W2(12));
      R(d, e, f, g, h, a, b, c, 13, W2(13));
      R(c, d, e, f, g, h, a, b, 14, W2(14));
      R(b, c, d, e, f, g, h, a, 15, W2(15));

      R(a, b, c, d, e, f, g, h, 16, W2(16));
      R(h, a, b, c, d, e, f, g, 17, W2(17));
      R(g, h, a, b, c, d, e, f, 18, W2(18));
      R(f, g, h, a, b, c, d, e, 19, W2(19));
      R(e, f, g, h, a, b, c, d, 20, W2(20));
      R(d, e, f, g, h, a, b, c, 21, W2(21));
      R(c, d, e, f, g, h, a, b, 22, W2(22));
      R(b, c, d, e, f, g, h, a, 23, W2(23));
      R(a, b, c, d, e, f, g, h, 24, W2(24));
      R(h, a, b, c, d, e, f, g, 25, W2(25));
      R(g, h, a, b, c, d, e, f, 26, W2(26));
      R(f, g, h, a, b, c, d, e, 27, W2(27));
      R(e, f, g, h, a, b, c, d, 28, W2(28));
      R(d, e, f, g, h, a, b, c, 29, W2(29));
      R(c, d, e, f, g, h, a, b, 30, W2(30));
      R(b, c, d, e, f, g, h, a, 31, W2(31));

      R(a, b, c, d, e, f, g, h, 32, W2(32));
      R(h, a, b, c, d, e, f, g, 33, W2(33));
      R(g, h, a, b, c, d, e, f, 34, W2(34));
      R(f, g, h, a, b, c, d, e, 35, W2(35));
      R(e, f, g, h, a, b, c, d, 36, W2(36));
      R(d, e, f, g, h, a, b, c, 37, W2(37));
      R(c, d, e, f, g, h, a, b, 38, W2(38));
      R(b, c, d, e, f, g, h, a, 39, W2(39));
      R(a, b, c, d, e, f, g, h, 40, W2(40));
      R(h, a, b, c, d, e, f, g, 41, W2(41));
      R(g, h, a, b, c, d, e, f, 42, W2(42));
      R(f, g, h, a, b, c, d, e, 43, W2(43));
      R(e, f, g, h, a, b, c, d, 44, W2(44));
      R(d, e, f, g, h, a, b, c, 45, W2(45));
      R(c, d, e, f, g, h, a, b, 46, W2(46));
      R(b, c, d, e, f, g, h, a, 47, W2(47));

      R(a, b, c, d, e, f, g, h, 48, W2(48));
      R(h, a, b, c, d, e, f, g, 49, W2(49));
      R(g, h, a, b, c, d, e, f, 50, W2(50));
      R(f, g, h, a, b, c, d, e, 51, W2(51));
      R(e, f, g, h, a, b, c, d, 52, W2(52));
      R(d, e, f, g, h, a, b, c, 53, W2(53));
      R(c, d, e, f, g, h, a, b, 54, W2(54));
      R(b, c, d, e, f, g, h, a, 55, W2(55));
      R(a, b, c, d, e, f, g, h, 56, W2(56));
      R(h, a, b, c, d, e, f, g, 57, W2(57));
      R(g, h, a, b, c, d, e, f, 58, W2(58));
      R(f, g, h, a, b, c, d, e, 59, W2(59));
      R(e, f, g, h, a, b, c, d, 60, W2(60));
      R(d, e, f, g, h, a, b, c, 61, W2(61));
      R(c, d, e, f, g, h, a, b, 62, W2(62));
      R(b, c, d, e, f, g, h, a, 63, W2(63));

      R(a, b, c, d, e, f, g, h, 64, L2(64));
      R(h, a, b, c, d, e, f, g, 65, L2(65));
      R(g, h, a, b, c, d, e, f, 66, L2(66));
      R(f, g, h, a, b, c, d, e, 67, L2(67));
      R(e, f, g, h, a, b, c, d, 68, L2(68));
      R(d, e, f, g, h, a, b, c, 69, L2(69));
      R(c, d, e, f, g, h, a, b, 70, L2(70));
      R(b, c, d, e, f, g, h, a, 71, L2(71));
      R(a, b, c, d, e, f, g, h, 72, L2(72));
      R(h, a, b, c, d, e, f, g, 73, L2(73));
      R(g, h, a, b, c, d, e, f, 74, L2(74));
      R(f, g, h, a, b, c, d, e, 75, L2(75));
      R(e, f, g, h, a, b, c, d, 76, L2(76));
      R(d, e, f, g, h, a, b, c, 77, L2(77));
      R(c, d, e, f, g, h, a, b, 78, L2(78));
      R(b, c, d, e, f, g, h, a, 79, L2(79));

      h0 += a;
      h1 += b;
      h2 += c;
      h3 += d;
      h4 += e;
      h5 += f;
      h6 += g;
      h7 += h;

      kptr = K;

      a = h0;
      b = h1;
      c = h2;
      d = h3;
      e = h4;
      f = h5;
      g = h6;
      h = h7;

      R(a, b, c, d, e, f, g, h, 0, WL(0));
      R(h, a, b, c, d, e, f, g, 1, WL(1));
      R(g, h, a, b, c, d, e, f, 2, WL(2));
      R(f, g, h, a, b, c, d, e, 3, WL(3));
      R(e, f, g, h, a, b, c, d, 4, WL(4));
      R(d, e, f, g, h, a, b, c, 5, WL(5));
      R(c, d, e, f, g, h, a, b, 6, WL(6));
      R(b, c, d, e, f, g, h, a, 7, WL(7));

      R(a, b, c, d, e, f, g, h, 8, WL(8));
      R(h, a, b, c, d, e, f, g, 9, WL(9));
      R(g, h, a, b, c, d, e, f, 10, WL(10));
      R(f, g, h, a, b, c, d, e, 11, WL(11));
      R(e, f, g, h, a, b, c, d, 12, WL(12));
      R(d, e, f, g, h, a, b, c, 13, WL(13));
      R(c, d, e, f, g, h, a, b, 14, WL(14));
      R(b, c, d, e, f, g, h, a, 15, WL(15));

      R(a, b, c, d, e, f, g, h, 16, WL(16));
      R(h, a, b, c, d, e, f, g, 17, WL(17));
      R(g, h, a, b, c, d, e, f, 18, WL(18));
      R(f, g, h, a, b, c, d, e, 19, WL(19));
      R(e, f, g, h, a, b, c, d, 20, WL(20));
      R(d, e, f, g, h, a, b, c, 21, WL(21));
      R(c, d, e, f, g, h, a, b, 22, WL(22));
      R(b, c, d, e, f, g, h, a, 23, WL(23));
      R(a, b, c, d, e, f, g, h, 24, WL(24));
      R(h, a, b, c, d, e, f, g, 25, WL(25));
      R(g, h, a, b, c, d, e, f, 26, WL(26));
      R(f, g, h, a, b, c, d, e, 27, WL(27));
      R(e, f, g, h, a, b, c, d, 28, WL(28));
      R(d, e, f, g, h, a, b, c, 29, WL(29));
      R(c, d, e, f, g, h, a, b, 30, WL(30));
      R(b, c, d, e, f, g, h, a, 31, WL(31));

      R(a, b, c, d, e, f, g, h, 32, WL(32));
      R(h, a, b, c, d, e, f, g, 33, WL(33));
      R(g, h, a, b, c, d, e, f, 34, WL(34));
      R(f, g, h, a, b, c, d, e, 35, WL(35));
      R(e, f, g, h, a, b, c, d, 36, WL(36));
      R(d, e, f, g, h, a, b, c, 37, WL(37));
      R(c, d, e, f, g, h, a, b, 38, WL(38));
      R(b, c, d, e, f, g, h, a, 39, WL(39));
      R(a, b, c, d, e, f, g, h, 40, WL(40));
      R(h, a, b, c, d, e, f, g, 41, WL(41));
      R(g, h, a, b, c, d, e, f, 42, WL(42));
      R(f, g, h, a, b, c, d, e, 43, WL(43));
      R(e, f, g, h, a, b, c, d, 44, WL(44));
      R(d, e, f, g, h, a, b, c, 45, WL(45));
      R(c, d, e, f, g, h, a, b, 46, WL(46));
      R(b, c, d, e, f, g, h, a, 47, WL(47));

      R(a, b, c, d, e, f, g, h, 48, WL(48));
      R(h, a, b, c, d, e, f, g, 49, WL(49));
      R(g, h, a, b, c, d, e, f, 50, WL(50));
      R(f, g, h, a, b, c, d, e, 51, WL(51));
      R(e, f, g, h, a, b, c, d, 52, WL(52));
      R(d, e, f, g, h, a, b, c, 53, WL(53));
      R(c, d, e, f, g, h, a, b, 54, WL(54));
      R(b, c, d, e, f, g, h, a, 55, WL(55));
      R(a, b, c, d, e, f, g, h, 56, WL(56));
      R(h, a, b, c, d, e, f, g, 57, WL(57));
      R(g, h, a, b, c, d, e, f, 58, WL(58));
      R(f, g, h, a, b, c, d, e, 59, WL(59));
      R(e, f, g, h, a, b, c, d, 60, WL(60));
      R(d, e, f, g, h, a, b, c, 61, WL(61));
      R(c, d, e, f, g, h, a, b, 62, WL(62));
      R(b, c, d, e, f, g, h, a, 63, WL(63));

      R(a, b, c, d, e, f, g, h, 64, WL(64));
      R(h, a, b, c, d, e, f, g, 65, WL(65));
      R(g, h, a, b, c, d, e, f, 66, WL(66));
      R(f, g, h, a, b, c, d, e, 67, WL(67));
      R(e, f, g, h, a, b, c, d, 68, WL(68));
      R(d, e, f, g, h, a, b, c, 69, WL(69));
      R(c, d, e, f, g, h, a, b, 70, WL(70));
      R(b, c, d, e, f, g, h, a, 71, WL(71));
      R(a, b, c, d, e, f, g, h, 72, WL(72));
      R(h, a, b, c, d, e, f, g, 73, WL(73));
      R(g, h, a, b, c, d, e, f, 74, WL(74));
      R(f, g, h, a, b, c, d, e, 75, WL(75));
      R(e, f, g, h, a, b, c, d, 76, WL(76));
      R(d, e, f, g, h, a, b, c, 77, WL(77));
      R(c, d, e, f, g, h, a, b, 78, WL(78));
      R(b, c, d, e, f, g, h, a, 79, WL(79));

      h0 += a;
      h1 += b;
      h2 += c;
      h3 += d;
      h4 += e;
      h5 += f;
      h6 += g;
      h7 += h;

      nblks -= 2;
    }

  if (nblks)
    {
      const vector2x_u64 *kptr = K;
      vector2x_u64 ktmp;

      a = h0;
      b = h1;
      c = h2;
      d = h3;
      e = h4;
      f = h5;
      g = h6;
      h = h7;

      I(0); I(1); I(2); I(3);
      I(4); I(5); I(6); I(7);
      I(8); I(9); I(10); I(11);
      I(12); I(13); I(14); I(15);

      R(a, b, c, d, e, f, g, h, 0, W(0));
      R(h, a, b, c, d, e, f, g, 1, W(1));
      R(g, h, a, b, c, d, e, f, 2, W(2));
      R(f, g, h, a, b, c, d, e, 3, W(3));
      R(e, f, g, h, a, b, c, d, 4, W(4));
      R(d, e, f, g, h, a, b, c, 5, W(5));
      R(c, d, e, f, g, h, a, b, 6, W(6));
      R(b, c, d, e, f, g, h, a, 7, W(7));

      R(a, b, c, d, e, f, g, h, 8, W(8));
      R(h, a, b, c, d, e, f, g, 9, W(9));
      R(g, h, a, b, c, d, e, f, 10, W(10));
      R(f, g, h, a, b, c, d, e, 11, W(11));
      R(e, f, g, h, a, b, c, d, 12, W(12));
      R(d, e, f, g, h, a, b, c, 13, W(13));
      R(c, d, e, f, g, h, a, b, 14, W(14));
      R(b, c, d, e, f, g, h, a, 15, W(15));

      R(a, b, c, d, e, f, g, h, 16, W(16));
      R(h, a, b, c, d, e, f, g, 17, W(17));
      R(g, h, a, b, c, d, e, f, 18, W(18));
      R(f, g, h, a, b, c, d, e, 19, W(19));
      R(e, f, g, h, a, b, c, d, 20, W(20));
      R(d, e, f, g, h, a, b, c, 21, W(21));
      R(c, d, e, f, g, h, a, b, 22, W(22));
      R(b, c, d, e, f, g, h, a, 23, W(23));
      R(a, b, c, d, e, f, g, h, 24, W(24));
      R(h, a, b, c, d, e, f, g, 25, W(25));
      R(g, h, a, b, c, d, e, f, 26, W(26));
      R(f, g, h, a, b, c, d, e, 27, W(27));
      R(e, f, g, h, a, b, c, d, 28, W(28));
      R(d, e, f, g, h, a, b, c, 29, W(29));
      R(c, d, e, f, g, h, a, b, 30, W(30));
      R(b, c, d, e, f, g, h, a, 31, W(31));

      R(a, b, c, d, e, f, g, h, 32, W(32));
      R(h, a, b, c, d, e, f, g, 33, W(33));
      R(g, h, a, b, c, d, e, f, 34, W(34));
      R(f, g, h, a, b, c, d, e, 35, W(35));
      R(e, f, g, h, a, b, c, d, 36, W(36));
      R(d, e, f, g, h, a, b, c, 37, W(37));
      R(c, d, e, f, g, h, a, b, 38, W(38));
      R(b, c, d, e, f, g, h, a, 39, W(39));
      R(a, b, c, d, e, f, g, h, 40, W(40));
      R(h, a, b, c, d, e, f, g, 41, W(41));
      R(g, h, a, b, c, d, e, f, 42, W(42));
      R(f, g, h, a, b, c, d, e, 43, W(43));
      R(e, f, g, h, a, b, c, d, 44, W(44));
      R(d, e, f, g, h, a, b, c, 45, W(45));
      R(c, d, e, f, g, h, a, b, 46, W(46));
      R(b, c, d, e, f, g, h, a, 47, W(47));

      R(a, b, c, d, e, f, g, h, 48, W(48));
      R(h, a, b, c, d, e, f, g, 49, W(49));
      R(g, h, a, b, c, d, e, f, 50, W(50));
      R(f, g, h, a, b, c, d, e, 51, W(51));
      R(e, f, g, h, a, b, c, d, 52, W(52));
      R(d, e, f, g, h, a, b, c, 53, W(53));
      R(c, d, e, f, g, h, a, b, 54, W(54));
      R(b, c, d, e, f, g, h, a, 55, W(55));
      R(a, b, c, d, e, f, g, h, 56, W(56));
      R(h, a, b, c, d, e, f, g, 57, W(57));
      R(g, h, a, b, c, d, e, f, 58, W(58));
      R(f, g, h, a, b, c, d, e, 59, W(59));
      R(e, f, g, h, a, b, c, d, 60, W(60));
      R(d, e, f, g, h, a, b, c, 61, W(61));
      R(c, d, e, f, g, h, a, b, 62, W(62));
      R(b, c, d, e, f, g, h, a, 63, W(63));

      R(a, b, c, d, e, f, g, h, 64, L(64));
      R(h, a, b, c, d, e, f, g, 65, L(65));
      R(g, h, a, b, c, d, e, f, 66, L(66));
      R(f, g, h, a, b, c, d, e, 67, L(67));
      R(e, f, g, h, a, b, c, d, 68, L(68));
      R(d, e, f, g, h, a, b, c, 69, L(69));
      R(c, d, e, f, g, h, a, b, 70, L(70));
      R(b, c, d, e, f, g, h, a, 71, L(71));
      R(a, b, c, d, e, f, g, h, 72, L(72));
      R(h, a, b, c, d, e, f, g, 73, L(73));
      R(g, h, a, b, c, d, e, f, 74, L(74));
      R(f, g, h, a, b, c, d, e, 75, L(75));
      R(e, f, g, h, a, b, c, d, 76, L(76));
      R(d, e, f, g, h, a, b, c, 77, L(77));
      R(c, d, e, f, g, h, a, b, 78, L(78));
      R(b, c, d, e, f, g, h, a, 79, L(79));

      h0 += a;
      h1 += b;
      h2 += c;
      h3 += d;
      h4 += e;
      h5 += f;
      h6 += g;
      h7 += h;

      nblks--;
    }

  h0 = vec_merge_idx0_elems (h0, h1);
  h2 = vec_merge_idx0_elems (h2, h3);
  h4 = vec_merge_idx0_elems (h4, h5);
  h6 = vec_merge_idx0_elems (h6, h7);
  vec_u64_store (h0, 8 * 0, (unsigned long long *)state);
  vec_u64_store (h2, 8 * 2, (unsigned long long *)state);
  vec_u64_store (h4, 8 * 4, (unsigned long long *)state);
  vec_u64_store (h6, 8 * 6, (unsigned long long *)state);

  return sizeof(w) + sizeof(w2);
}

unsigned int ASM_FUNC_ATTR FUNC_ATTR_TARGET_P8 FUNC_ATTR_OPT_O2
_gcry_sha512_transform_ppc8(u64 state[8], const unsigned char *data,
			    size_t nblks)
{
  return sha512_transform_ppc(state, data, nblks);
}

unsigned int ASM_FUNC_ATTR FUNC_ATTR_TARGET_P9 FUNC_ATTR_OPT_O2
_gcry_sha512_transform_ppc9(u64 state[8], const unsigned char *data,
			    size_t nblks)
{
  return sha512_transform_ppc(state, data, nblks);
}

#endif /* ENABLE_PPC_CRYPTO_SUPPORT */
