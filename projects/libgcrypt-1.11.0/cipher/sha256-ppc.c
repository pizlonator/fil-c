/* sha256-ppc.c - PowerPC vcrypto implementation of SHA-256 transform
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
    defined(USE_SHA256) && \
    __GNUC__ >= 4

#include <altivec.h>
#include "bufhelp.h"


typedef vector unsigned char vector16x_u8;
typedef vector unsigned int vector4x_u32;
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


static const vector4x_u32 K[64 / 4] =
  {
#define TBL(v) v
    { TBL(0x428a2f98), TBL(0x71374491), TBL(0xb5c0fbcf), TBL(0xe9b5dba5) },
    { TBL(0x3956c25b), TBL(0x59f111f1), TBL(0x923f82a4), TBL(0xab1c5ed5) },
    { TBL(0xd807aa98), TBL(0x12835b01), TBL(0x243185be), TBL(0x550c7dc3) },
    { TBL(0x72be5d74), TBL(0x80deb1fe), TBL(0x9bdc06a7), TBL(0xc19bf174) },
    { TBL(0xe49b69c1), TBL(0xefbe4786), TBL(0x0fc19dc6), TBL(0x240ca1cc) },
    { TBL(0x2de92c6f), TBL(0x4a7484aa), TBL(0x5cb0a9dc), TBL(0x76f988da) },
    { TBL(0x983e5152), TBL(0xa831c66d), TBL(0xb00327c8), TBL(0xbf597fc7) },
    { TBL(0xc6e00bf3), TBL(0xd5a79147), TBL(0x06ca6351), TBL(0x14292967) },
    { TBL(0x27b70a85), TBL(0x2e1b2138), TBL(0x4d2c6dfc), TBL(0x53380d13) },
    { TBL(0x650a7354), TBL(0x766a0abb), TBL(0x81c2c92e), TBL(0x92722c85) },
    { TBL(0xa2bfe8a1), TBL(0xa81a664b), TBL(0xc24b8b70), TBL(0xc76c51a3) },
    { TBL(0xd192e819), TBL(0xd6990624), TBL(0xf40e3585), TBL(0x106aa070) },
    { TBL(0x19a4c116), TBL(0x1e376c08), TBL(0x2748774c), TBL(0x34b0bcb5) },
    { TBL(0x391c0cb3), TBL(0x4ed8aa4a), TBL(0x5b9cca4f), TBL(0x682e6ff3) },
    { TBL(0x748f82ee), TBL(0x78a5636f), TBL(0x84c87814), TBL(0x8cc70208) },
    { TBL(0x90befffa), TBL(0xa4506ceb), TBL(0xbef9a3f7), TBL(0xc67178f2) }
#undef TBL
  };


static ASM_FUNC_ATTR_INLINE vector4x_u32
vec_rol_elems(vector4x_u32 v, unsigned int idx)
{
#ifndef WORDS_BIGENDIAN
  return vec_sld (v, v, (16 - (4 * idx)) & 15);
#else
  return vec_sld (v, v, (4 * idx) & 15);
#endif
}


static ASM_FUNC_ATTR_INLINE vector4x_u32
vec_merge_idx0_elems(vector4x_u32 v0, vector4x_u32 v1,
		     vector4x_u32 v2, vector4x_u32 v3)
{
  return (vector4x_u32)vec_mergeh ((vector2x_u64) vec_mergeh(v0, v1),
				   (vector2x_u64) vec_mergeh(v2, v3));
}


static ASM_FUNC_ATTR_INLINE vector4x_u32
vec_vshasigma_u32(vector4x_u32 v, unsigned int a, unsigned int b)
{
  asm ("vshasigmaw %0,%1,%2,%3"
       : "=v" (v)
       : "v" (v), "g" (a), "g" (b)
       : "memory");
  return v;
}


static ASM_FUNC_ATTR_INLINE vector4x_u32
vec_add_u32(vector4x_u32 v, vector4x_u32 w)
{
  __asm__ ("vadduwm %0,%1,%2"
	   : "=v" (v)
	   : "v" (v), "v" (w)
	   : "memory");
  return v;
}


static ASM_FUNC_ATTR_INLINE vector4x_u32
vec_u32_load_be(unsigned long offset, const void *ptr)
{
  vector4x_u32 vecu32;
#if __GNUC__ >= 4
  if (__builtin_constant_p (offset) && offset == 0)
    __asm__ volatile ("lxvw4x %x0,0,%1\n\t"
		      : "=wa" (vecu32)
		      : "r" ((uintptr_t)ptr)
		      : "memory");
  else
#endif
    __asm__ volatile ("lxvw4x %x0,%1,%2\n\t"
		      : "=wa" (vecu32)
		      : "r" (offset), "r" ((uintptr_t)ptr)
		      : "memory", "r0");
#ifndef WORDS_BIGENDIAN
  return (vector4x_u32)vec_reve((vector16x_u8)vecu32);
#else
  return vecu32;
#endif
}


/* SHA2 round in vector registers */
#define R(a,b,c,d,e,f,g,h,ki,w) do                            \
    {                                                         \
      t1 = vec_add_u32((h), (w));                             \
      t2 = Cho((e),(f),(g));                                  \
      t1 = vec_add_u32(t1, GETK(ki));                         \
      t1 = vec_add_u32(t1, t2);                               \
      t1 = Sum1add(t1, e);                                    \
      t2 = Maj((a),(b),(c));                                  \
      t2 = Sum0add(t2, a);                                    \
      h  = vec_add_u32(t1, t2);                               \
      d += t1;                                                \
    } while (0)

#define GETK(kidx) \
    ({ \
      vector4x_u32 rk; \
      if (((kidx) % 4) == 0) \
	{ \
	  rk = ktmp = *(kptr++); \
	  if ((kidx) < 63) \
	    asm volatile("" : "+r" (kptr) :: "memory"); \
	} \
      else if (((kidx) % 4) == 1) \
	{ \
	  rk = vec_mergeo(ktmp, ktmp); \
	} \
      else \
	{ \
	  rk = vec_rol_elems(ktmp, ((kidx) % 4)); \
	} \
      rk; \
    })

#define Cho(b, c, d)  (vec_sel(d, c, b))

#define Maj(c, d, b)  (vec_sel(c, b, c ^ d))

#define Sum0(x)       (vec_vshasigma_u32(x, 1, 0))

#define Sum1(x)       (vec_vshasigma_u32(x, 1, 15))

#define S0(x)         (vec_vshasigma_u32(x, 0, 0))

#define S1(x)         (vec_vshasigma_u32(x, 0, 15))

#define Xadd(X, d, x) vec_add_u32(d, X(x))

#define Sum0add(d, x) Xadd(Sum0, d, x)

#define Sum1add(d, x) Xadd(Sum1, d, x)

#define S0add(d, x)   Xadd(S0, d, x)

#define S1add(d, x)   Xadd(S1, d, x)

#define I(i) \
    ({ \
      if (((i) % 4) == 0) \
	{ \
	  w[i] = vec_u32_load_be(0, data); \
	  data += 4 * 4; \
	  if ((i) / 4 < 3) \
	    asm volatile("" : "+r"(data) :: "memory"); \
	} \
      else if (((i) % 4) == 1) \
	{ \
	  w[i] = vec_mergeo(w[(i) - 1], w[(i) - 1]); \
	} \
      else \
	{ \
	  w[i] = vec_rol_elems(w[(i) - (i) % 4], (i)); \
	} \
    })

#define WN(i) ({ w[(i)&0x0f] += w[((i)-7) &0x0f];  \
		 w[(i)&0x0f] = S0add(w[(i)&0x0f], w[((i)-15)&0x0f]); \
		 w[(i)&0x0f] = S1add(w[(i)&0x0f], w[((i)-2) &0x0f]); })

#define W(i) ({ vector4x_u32 r = w[(i)&0x0f]; WN(i); r; })

#define L(i) w[(i)&0x0f]

#define I2(i) \
    ({ \
      if ((i) % 4 == 0) \
	{ \
	  vector4x_u32 iw = vec_u32_load_be(0, data); \
	  vector4x_u32 iw2 = vec_u32_load_be(64, data); \
	  if ((i) / 4 < 3) \
	    { \
	      data += 4 * 4; \
	      asm volatile("" : "+r"(data) :: "memory"); \
	    } \
	  else \
	    { \
	      data += 4 * 4 + 64; \
	      asm volatile("" : "+r"(data) :: "memory"); \
	    } \
	  w[(i) + 0] = vec_mergeh(iw, iw2); \
	  w[(i) + 1] = vec_rol_elems(w[(i) + 0], 2); \
	  w[(i) + 2] = vec_mergel(iw, iw2); \
	  w[(i) + 3] = vec_rol_elems(w[(i) + 2], 2); \
	} \
    })

#define W2(i) \
    ({ \
      vector4x_u32 wt1 = w[(i)&0x0f]; \
      WN(i); \
      w2[(i) / 2] = (((i) % 2) == 0) ? wt1 : vec_mergeo(w2[(i) / 2], wt1); \
      wt1; \
    })

#define L2(i) \
    ({ \
      vector4x_u32 lt1 = w[(i)&0x0f]; \
      w2[(i) / 2] = (((i) % 2) == 0) ? lt1 : vec_mergeo(w2[(i) / 2], lt1); \
      lt1; \
    })

#define WL(i) \
    ({ \
      vector4x_u32 wlt1 = w2[(i) / 2]; \
      if (((i) % 2) == 0 && (i) < 63) \
	w2[(i) / 2] = vec_mergeo(wlt1, wlt1); \
      wlt1; \
    })

static ASM_FUNC_ATTR_INLINE FUNC_ATTR_OPT_O2 unsigned int
sha256_transform_ppc(u32 state[8], const unsigned char *data, size_t nblks)
{
  vector4x_u32 h0, h1, h2, h3, h4, h5, h6, h7;
  vector4x_u32 h0_h3, h4_h7;
  vector4x_u32 a, b, c, d, e, f, g, h, t1, t2;
  vector4x_u32 w[16];
  vector4x_u32 w2[64 / 2];

  h0_h3 = vec_vsx_ld (4 * 0, state);
  h4_h7 = vec_vsx_ld (4 * 4, state);

  h0 = h0_h3;
  h1 = vec_mergeo (h0_h3, h0_h3);
  h2 = vec_rol_elems (h0_h3, 2);
  h3 = vec_rol_elems (h0_h3, 3);
  h4 = h4_h7;
  h5 = vec_mergeo (h4_h7, h4_h7);
  h6 = vec_rol_elems (h4_h7, 2);
  h7 = vec_rol_elems (h4_h7, 3);

  while (nblks >= 2)
    {
      const vector4x_u32 *kptr = K;
      vector4x_u32 ktmp;

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

      R(a, b, c, d, e, f, g, h, 48, L2(48));
      R(h, a, b, c, d, e, f, g, 49, L2(49));
      R(g, h, a, b, c, d, e, f, 50, L2(50));
      R(f, g, h, a, b, c, d, e, 51, L2(51));
      R(e, f, g, h, a, b, c, d, 52, L2(52));
      R(d, e, f, g, h, a, b, c, 53, L2(53));
      R(c, d, e, f, g, h, a, b, 54, L2(54));
      R(b, c, d, e, f, g, h, a, 55, L2(55));
      R(a, b, c, d, e, f, g, h, 56, L2(56));
      R(h, a, b, c, d, e, f, g, 57, L2(57));
      R(g, h, a, b, c, d, e, f, 58, L2(58));
      R(f, g, h, a, b, c, d, e, 59, L2(59));
      R(e, f, g, h, a, b, c, d, 60, L2(60));
      R(d, e, f, g, h, a, b, c, 61, L2(61));
      R(c, d, e, f, g, h, a, b, 62, L2(62));
      R(b, c, d, e, f, g, h, a, 63, L2(63));

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
      const vector4x_u32 *kptr = K;
      vector4x_u32 ktmp;

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

      R(a, b, c, d, e, f, g, h, 48, L(48));
      R(h, a, b, c, d, e, f, g, 49, L(49));
      R(g, h, a, b, c, d, e, f, 50, L(50));
      R(f, g, h, a, b, c, d, e, 51, L(51));
      R(e, f, g, h, a, b, c, d, 52, L(52));
      R(d, e, f, g, h, a, b, c, 53, L(53));
      R(c, d, e, f, g, h, a, b, 54, L(54));
      R(b, c, d, e, f, g, h, a, 55, L(55));
      R(a, b, c, d, e, f, g, h, 56, L(56));
      R(h, a, b, c, d, e, f, g, 57, L(57));
      R(g, h, a, b, c, d, e, f, 58, L(58));
      R(f, g, h, a, b, c, d, e, 59, L(59));
      R(e, f, g, h, a, b, c, d, 60, L(60));
      R(d, e, f, g, h, a, b, c, 61, L(61));
      R(c, d, e, f, g, h, a, b, 62, L(62));
      R(b, c, d, e, f, g, h, a, 63, L(63));

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

  h0_h3 = vec_merge_idx0_elems (h0, h1, h2, h3);
  h4_h7 = vec_merge_idx0_elems (h4, h5, h6, h7);
  vec_vsx_st (h0_h3, 4 * 0, state);
  vec_vsx_st (h4_h7, 4 * 4, state);

  return sizeof(w2) + sizeof(w);
}

unsigned int ASM_FUNC_ATTR FUNC_ATTR_TARGET_P8 FUNC_ATTR_OPT_O2
_gcry_sha256_transform_ppc8(u32 state[8], const unsigned char *data,
			    size_t nblks)
{
  return sha256_transform_ppc(state, data, nblks);
}

unsigned int ASM_FUNC_ATTR FUNC_ATTR_TARGET_P9 FUNC_ATTR_OPT_O2
_gcry_sha256_transform_ppc9(u32 state[8], const unsigned char *data,
			    size_t nblks)
{
  return sha256_transform_ppc(state, data, nblks);
}

#endif /* ENABLE_PPC_CRYPTO_SUPPORT */
