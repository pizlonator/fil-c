/* sntrup761.c  -  Streamlined NTRU Prime sntrup761 key-encapsulation method
 * Copyright (C) 2023 Simon Josefsson <simon@josefsson.org>
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
 * License along with this program; if not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * For a description of the algorithm, see:
 *   https://ntruprime.cr.yp.to/
 */

/*
 * Derived from public domain source, written by (in alphabetical order):
 * - Daniel J. Bernstein
 * - Chitchanok Chuengsatiansup
 * - Tanja Lange
 * - Christine van Vredendaal
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "sntrup761.h"

/* from supercop-20201130/crypto_sort/int32/portable4/int32_minmax.inc */
#define int32_MINMAX(a,b) \
do { \
  int64_t ab = (int64_t)b ^ (int64_t)a; \
  int64_t c = (int64_t)b - (int64_t)a; \
  c ^= ab & (c ^ b); \
  c >>= 31; \
  c &= ab; \
  a ^= c; \
  b ^= c; \
} while(0)

/* from supercop-20201130/crypto_sort/int32/portable4/sort.c */
static void
crypto_sort_int32 (void *array, long long n)
{
  long long top, p, q, r, i, j;
  int32_t *x = array;

  if (n < 2)
    return;
  top = 1;
  while (top < n - top)
    top += top;

  for (p = top; p >= 1; p >>= 1)
    {
      i = 0;
      while (i + 2 * p <= n)
	{
	  for (j = i; j < i + p; ++j)
	    int32_MINMAX (x[j], x[j + p]);
	  i += 2 * p;
	}
      for (j = i; j < n - p; ++j)
	int32_MINMAX (x[j], x[j + p]);

      i = 0;
      j = 0;
      for (q = top; q > p; q >>= 1)
	{
	  if (j != i)
	    for (;;)
	      {
                int32_t a;

		if (j == n - q)
		  goto done;
		a = x[j + p];
		for (r = q; r > p; r >>= 1)
		  int32_MINMAX (a, x[j + r]);
		x[j + p] = a;
		++j;
		if (j == i + p)
		  {
		    i += 2 * p;
		    break;
		  }
	      }
	  while (i + p <= n - q)
	    {
	      for (j = i; j < i + p; ++j)
		{
		  int32_t a = x[j + p];
		  for (r = q; r > p; r >>= 1)
		    int32_MINMAX (a, x[j + r]);
		  x[j + p] = a;
		}
	      i += 2 * p;
	    }
	  /* now i + p > n - q */
	  j = i;
	  while (j < n - q)
	    {
	      int32_t a = x[j + p];
	      for (r = q; r > p; r >>= 1)
		int32_MINMAX (a, x[j + r]);
	      x[j + p] = a;
	      ++j;
	    }

	done:;
	}
    }
}

/* from supercop-20201130/crypto_sort/uint32/useint32/sort.c */

/* can save time by vectorizing xor loops */
/* can save time by integrating xor loops with int32_sort */

static void
crypto_sort_uint32 (void *array, long long n)
{
  uint32_t *x = array;
  long long j;
  for (j = 0; j < n; ++j)
    x[j] ^= 0x80000000;
  crypto_sort_int32 (array, n);
  for (j = 0; j < n; ++j)
    x[j] ^= 0x80000000;
}

/* from supercop-20201130/crypto_kem/sntrup761/ref/uint32.c */

/*
CPU division instruction typically takes time depending on x.
This software is designed to take time independent of x.
Time still varies depending on m; user must ensure that m is constant.
Time also varies on CPUs where multiplication is variable-time.
There could be more CPU issues.
There could also be compiler issues.
*/

static void
uint32_divmod_uint14 (uint32_t * q, uint16_t * r, uint32_t x, uint16_t m)
{
  uint32_t v = 0x80000000;
  uint32_t qpart;
  uint32_t mask;

  v /= m;

  /* caller guarantees m > 0 */
  /* caller guarantees m < 16384 */
  /* vm <= 2^31 <= vm+m-1 */
  /* xvm <= 2^31 x <= xvm+x(m-1) */

  *q = 0;

  qpart = (x * (uint64_t) v) >> 31;
  /* 2^31 qpart <= xv <= 2^31 qpart + 2^31-1 */
  /* 2^31 qpart m <= xvm <= 2^31 qpart m + (2^31-1)m */
  /* 2^31 qpart m <= 2^31 x <= 2^31 qpart m + (2^31-1)m + x(m-1) */
  /* 0 <= 2^31 newx <= (2^31-1)m + x(m-1) */
  /* 0 <= newx <= (1-1/2^31)m + x(m-1)/2^31 */
  /* 0 <= newx <= (1-1/2^31)(2^14-1) + (2^32-1)((2^14-1)-1)/2^31 */

  x -= qpart * m;
  *q += qpart;
  /* x <= 49146 */

  qpart = (x * (uint64_t) v) >> 31;
  /* 0 <= newx <= (1-1/2^31)m + x(m-1)/2^31 */
  /* 0 <= newx <= m + 49146(2^14-1)/2^31 */
  /* 0 <= newx <= m + 0.4 */
  /* 0 <= newx <= m */

  x -= qpart * m;
  *q += qpart;
  /* x <= m */

  x -= m;
  *q += 1;
  mask = -(x >> 31);
  x += mask & (uint32_t) m;
  *q += mask;
  /* x < m */

  *r = x;
}


static uint16_t
uint32_mod_uint14 (uint32_t x, uint16_t m)
{
  uint32_t q;
  uint16_t r;
  uint32_divmod_uint14 (&q, &r, x, m);
  return r;
}

/* from supercop-20201130/crypto_kem/sntrup761/ref/int32.c */

static void
int32_divmod_uint14 (int32_t * q, uint16_t * r, int32_t x, uint16_t m)
{
  uint32_t uq, uq2;
  uint16_t ur, ur2;
  uint32_t mask;

  uint32_divmod_uint14 (&uq, &ur, 0x80000000 + (uint32_t) x, m);
  uint32_divmod_uint14 (&uq2, &ur2, 0x80000000, m);
  ur -= ur2;
  uq -= uq2;
  mask = -(uint32_t) (ur >> 15);
  ur += mask & m;
  uq += mask;
  *r = ur;
  *q = uq;
}


static uint16_t
int32_mod_uint14 (int32_t x, uint16_t m)
{
  int32_t q;
  uint16_t r;
  int32_divmod_uint14 (&q, &r, x, m);
  return r;
}

/* from supercop-20201130/crypto_kem/sntrup761/ref/paramsmenu.h */
#define p 761
#define q 4591
#define Rounded_bytes 1007
#define Rq_bytes 1158
#define w 286

/* from supercop-20201130/crypto_kem/sntrup761/ref/Decode.h */

/* Decode(R,s,M,len) */
/* assumes 0 < M[i] < 16384 */
/* produces 0 <= R[i] < M[i] */

/* from supercop-20201130/crypto_kem/sntrup761/ref/Decode.c */

static void
Decode (uint16_t * out, const unsigned char *S, const uint16_t * M,
	long long len)
{
  if (len == 1)
    {
      if (M[0] == 1)
	*out = 0;
      else if (M[0] <= 256)
	*out = uint32_mod_uint14 (S[0], M[0]);
      else
	*out = uint32_mod_uint14 (S[0] + (((uint16_t) S[1]) << 8), M[0]);
    }
  if (len > 1)
    {
      uint16_t R2[(len + 1) / 2];
      uint16_t M2[(len + 1) / 2];
      uint16_t bottomr[len / 2];
      uint32_t bottomt[len / 2];
      long long i;
      for (i = 0; i < len - 1; i += 2)
	{
	  uint32_t m = M[i] * (uint32_t) M[i + 1];
	  if (m > 256 * 16383)
	    {
	      bottomt[i / 2] = 256 * 256;
	      bottomr[i / 2] = S[0] + 256 * S[1];
	      S += 2;
	      M2[i / 2] = (((m + 255) >> 8) + 255) >> 8;
	    }
	  else if (m >= 16384)
	    {
	      bottomt[i / 2] = 256;
	      bottomr[i / 2] = S[0];
	      S += 1;
	      M2[i / 2] = (m + 255) >> 8;
	    }
	  else
	    {
	      bottomt[i / 2] = 1;
	      bottomr[i / 2] = 0;
	      M2[i / 2] = m;
	    }
	}
      if (i < len)
	M2[i / 2] = M[i];
      Decode (R2, S, M2, (len + 1) / 2);
      for (i = 0; i < len - 1; i += 2)
	{
	  uint32_t r = bottomr[i / 2];
	  uint32_t r1;
	  uint16_t r0;
	  r += bottomt[i / 2] * R2[i / 2];
	  uint32_divmod_uint14 (&r1, &r0, r, M[i]);
	  r1 = uint32_mod_uint14 (r1, M[i + 1]);	/* only needed for invalid inputs */
	  *out++ = r0;
	  *out++ = r1;
	}
      if (i < len)
	*out++ = R2[i / 2];
    }
}

/* from supercop-20201130/crypto_kem/sntrup761/ref/Encode.h */

/* Encode(s,R,M,len) */
/* assumes 0 <= R[i] < M[i] < 16384 */

/* from supercop-20201130/crypto_kem/sntrup761/ref/Encode.c */

/* 0 <= R[i] < M[i] < 16384 */
static void
Encode (unsigned char *out, const uint16_t * R, const uint16_t * M,
	long long len)
{
  if (len == 1)
    {
      uint16_t r = R[0];
      uint16_t m = M[0];
      while (m > 1)
	{
	  *out++ = r;
	  r >>= 8;
	  m = (m + 255) >> 8;
	}
    }
  if (len > 1)
    {
      uint16_t R2[(len + 1) / 2];
      uint16_t M2[(len + 1) / 2];
      long long i;
      for (i = 0; i < len - 1; i += 2)
	{
	  uint32_t m0 = M[i];
	  uint32_t r = R[i] + R[i + 1] * m0;
	  uint32_t m = M[i + 1] * m0;
	  while (m >= 16384)
	    {
	      *out++ = r;
	      r >>= 8;
	      m = (m + 255) >> 8;
	    }
	  R2[i / 2] = r;
	  M2[i / 2] = m;
	}
      if (i < len)
	{
	  R2[i / 2] = R[i];
	  M2[i / 2] = M[i];
	}
      Encode (out, R2, M2, (len + 1) / 2);
    }
}

/* from supercop-20201130/crypto_kem/sntrup761/ref/kem.c */

/* ----- masks */

/* return -1 if x!=0; else return 0 */
static int
int16_t_nonzero_mask (int16_t x)
{
  uint16_t u = x;		/* 0, else 1...65535 */
  uint32_t v = u;		/* 0, else 1...65535 */
  v = -v;			/* 0, else 2^32-65535...2^32-1 */
  v >>= 31;			/* 0, else 1 */
  return -v;			/* 0, else -1 */
}

/* return -1 if x<0; otherwise return 0 */
static int
int16_t_negative_mask (int16_t x)
{
  uint16_t u = x;
  u >>= 15;
  return -(int) u;
  /* alternative with gcc -fwrapv: */
  /* x>>15 compiles to CPU's arithmetic right shift */
}

/* ----- arithmetic mod 3 */

typedef int8_t small;

/* F3 is always represented as -1,0,1 */
/* so ZZ_fromF3 is a no-op */

/* x must not be close to top int16_t */
static small
F3_freeze (int16_t x)
{
  return int32_mod_uint14 (x + 1, 3) - 1;
}

/* ----- arithmetic mod q */

#define q12 ((q-1)/2)
typedef int16_t Fq;
/* always represented as -q12...q12 */
/* so ZZ_fromFq is a no-op */

/* x must not be close to top int32 */
static Fq
Fq_freeze (int32_t x)
{
  return int32_mod_uint14 (x + q12, q) - q12;
}

static Fq
Fq_recip (Fq a1)
{
  int i = 1;
  Fq ai = a1;

  while (i < q - 2)
    {
      ai = Fq_freeze (a1 * (int32_t) ai);
      i += 1;
    }
  return ai;
}

/* ----- small polynomials */

/* 0 if Weightw_is(r), else -1 */
static int
Weightw_mask (small * r)
{
  int weight = 0;
  int i;

  for (i = 0; i < p; ++i)
    weight += r[i] & 1;
  return int16_t_nonzero_mask (weight - w);
}

/* R3_fromR(R_fromRq(r)) */
static void
R3_fromRq (small * out, const Fq * r)
{
  int i;
  for (i = 0; i < p; ++i)
    out[i] = F3_freeze (r[i]);
}

/* h = f*g in the ring R3 */
static void
R3_mult (small * h, const small * f, const small * g)
{
  small fg[p + p - 1];
  small result;
  int i, j;

  for (i = 0; i < p; ++i)
    {
      result = 0;
      for (j = 0; j <= i; ++j)
	result = F3_freeze (result + f[j] * g[i - j]);
      fg[i] = result;
    }
  for (i = p; i < p + p - 1; ++i)
    {
      result = 0;
      for (j = i - p + 1; j < p; ++j)
	result = F3_freeze (result + f[j] * g[i - j]);
      fg[i] = result;
    }

  for (i = p + p - 2; i >= p; --i)
    {
      fg[i - p] = F3_freeze (fg[i - p] + fg[i]);
      fg[i - p + 1] = F3_freeze (fg[i - p + 1] + fg[i]);
    }

  for (i = 0; i < p; ++i)
    h[i] = fg[i];
}

/* returns 0 if recip succeeded; else -1 */
static int
R3_recip (small * out, const small * in)
{
  small f[p + 1], g[p + 1], v[p + 1], r[p + 1];
  int i, loop, delta;
  int sign, swap, t;

  for (i = 0; i < p + 1; ++i)
    v[i] = 0;
  for (i = 0; i < p + 1; ++i)
    r[i] = 0;
  r[0] = 1;
  for (i = 0; i < p; ++i)
    f[i] = 0;
  f[0] = 1;
  f[p - 1] = f[p] = -1;
  for (i = 0; i < p; ++i)
    g[p - 1 - i] = in[i];
  g[p] = 0;

  delta = 1;

  for (loop = 0; loop < 2 * p - 1; ++loop)
    {
      for (i = p; i > 0; --i)
	v[i] = v[i - 1];
      v[0] = 0;

      sign = -g[0] * f[0];
      swap = int16_t_negative_mask (-delta) & int16_t_nonzero_mask (g[0]);
      delta ^= swap & (delta ^ -delta);
      delta += 1;

      for (i = 0; i < p + 1; ++i)
	{
	  t = swap & (f[i] ^ g[i]);
	  f[i] ^= t;
	  g[i] ^= t;
	  t = swap & (v[i] ^ r[i]);
	  v[i] ^= t;
	  r[i] ^= t;
	}

      for (i = 0; i < p + 1; ++i)
	g[i] = F3_freeze (g[i] + sign * f[i]);
      for (i = 0; i < p + 1; ++i)
	r[i] = F3_freeze (r[i] + sign * v[i]);

      for (i = 0; i < p; ++i)
	g[i] = g[i + 1];
      g[p] = 0;
    }

  sign = f[0];
  for (i = 0; i < p; ++i)
    out[i] = sign * v[p - 1 - i];

  return int16_t_nonzero_mask (delta);
}

/* ----- polynomials mod q */

/* h = f*g in the ring Rq */
static void
Rq_mult_small (Fq * h, const Fq * f, const small * g)
{
  Fq fg[p + p - 1];
  Fq result;
  int i, j;

  for (i = 0; i < p; ++i)
    {
      result = 0;
      for (j = 0; j <= i; ++j)
	result = Fq_freeze (result + f[j] * (int32_t) g[i - j]);
      fg[i] = result;
    }
  for (i = p; i < p + p - 1; ++i)
    {
      result = 0;
      for (j = i - p + 1; j < p; ++j)
	result = Fq_freeze (result + f[j] * (int32_t) g[i - j]);
      fg[i] = result;
    }

  for (i = p + p - 2; i >= p; --i)
    {
      fg[i - p] = Fq_freeze (fg[i - p] + fg[i]);
      fg[i - p + 1] = Fq_freeze (fg[i - p + 1] + fg[i]);
    }

  for (i = 0; i < p; ++i)
    h[i] = fg[i];
}

/* h = 3f in Rq */
static void
Rq_mult3 (Fq * h, const Fq * f)
{
  int i;

  for (i = 0; i < p; ++i)
    h[i] = Fq_freeze (3 * f[i]);
}

/* out = 1/(3*in) in Rq */
/* returns 0 if recip succeeded; else -1 */
static int
Rq_recip3 (Fq * out, const small * in)
{
  Fq f[p + 1], g[p + 1], v[p + 1], r[p + 1];
  int i, loop, delta;
  int swap, t;
  int32_t f0, g0;
  Fq scale;

  for (i = 0; i < p + 1; ++i)
    v[i] = 0;
  for (i = 0; i < p + 1; ++i)
    r[i] = 0;
  r[0] = Fq_recip (3);
  for (i = 0; i < p; ++i)
    f[i] = 0;
  f[0] = 1;
  f[p - 1] = f[p] = -1;
  for (i = 0; i < p; ++i)
    g[p - 1 - i] = in[i];
  g[p] = 0;

  delta = 1;

  for (loop = 0; loop < 2 * p - 1; ++loop)
    {
      for (i = p; i > 0; --i)
	v[i] = v[i - 1];
      v[0] = 0;

      swap = int16_t_negative_mask (-delta) & int16_t_nonzero_mask (g[0]);
      delta ^= swap & (delta ^ -delta);
      delta += 1;

      for (i = 0; i < p + 1; ++i)
	{
	  t = swap & (f[i] ^ g[i]);
	  f[i] ^= t;
	  g[i] ^= t;
	  t = swap & (v[i] ^ r[i]);
	  v[i] ^= t;
	  r[i] ^= t;
	}

      f0 = f[0];
      g0 = g[0];
      for (i = 0; i < p + 1; ++i)
	g[i] = Fq_freeze (f0 * g[i] - g0 * f[i]);
      for (i = 0; i < p + 1; ++i)
	r[i] = Fq_freeze (f0 * r[i] - g0 * v[i]);

      for (i = 0; i < p; ++i)
	g[i] = g[i + 1];
      g[p] = 0;
    }

  scale = Fq_recip (f[0]);
  for (i = 0; i < p; ++i)
    out[i] = Fq_freeze (scale * (int32_t) v[p - 1 - i]);

  return int16_t_nonzero_mask (delta);
}

/* ----- rounded polynomials mod q */

static void
Round (Fq * out, const Fq * a)
{
  int i;
  for (i = 0; i < p; ++i)
    out[i] = a[i] - F3_freeze (a[i]);
}

/* ----- sorting to generate short polynomial */

static void
Short_fromlist (small * out, const uint32_t * in)
{
  uint32_t L[p];
  int i;

  for (i = 0; i < w; ++i)
    L[i] = in[i] & (uint32_t) - 2;
  for (i = w; i < p; ++i)
    L[i] = (in[i] & (uint32_t) - 3) | 1;
  crypto_sort_uint32 (L, p);
  for (i = 0; i < p; ++i)
    out[i] = (L[i] & 3) - 1;
}

/* ----- underlying hash function */

#define Hash_bytes 32

/* e.g., b = 0 means out = Hash0(in) */
static void
Hash_prefix (unsigned char *out, int b, const unsigned char *in, int inlen)
{
  unsigned char x[inlen + 1];
  unsigned char h[64];
  int i;

  x[0] = b;
  for (i = 0; i < inlen; ++i)
    x[i + 1] = in[i];
  crypto_hash_sha512 (h, x, inlen + 1);
  for (i = 0; i < 32; ++i)
    out[i] = h[i];
}

/* ----- higher-level randomness */

static uint32_t
urandom32 (void *random_ctx, sntrup761_random_func * random)
{
  unsigned char c[4];
  uint32_t out[4];

  random (random_ctx, 4, c);
  out[0] = (uint32_t) c[0];
  out[1] = ((uint32_t) c[1]) << 8;
  out[2] = ((uint32_t) c[2]) << 16;
  out[3] = ((uint32_t) c[3]) << 24;
  return out[0] + out[1] + out[2] + out[3];
}

static void
Short_random (small * out, void *random_ctx, sntrup761_random_func * random)
{
  uint32_t L[p];
  int i;

  for (i = 0; i < p; ++i)
    L[i] = urandom32 (random_ctx, random);
  Short_fromlist (out, L);
}

static void
Small_random (small * out, void *random_ctx, sntrup761_random_func * random)
{
  int i;

  for (i = 0; i < p; ++i)
    out[i] = (((urandom32 (random_ctx, random) & 0x3fffffff) * 3) >> 30) - 1;
}

/* ----- Streamlined NTRU Prime Core */

/* h,(f,ginv) = KeyGen() */
static void
KeyGen (Fq * h, small * f, small * ginv, void *random_ctx,
	sntrup761_random_func * random)
{
  small g[p];
  Fq finv[p];

  for (;;)
    {
      Small_random (g, random_ctx, random);
      if (R3_recip (ginv, g) == 0)
	break;
    }
  Short_random (f, random_ctx, random);
  Rq_recip3 (finv, f);		/* always works */
  Rq_mult_small (h, finv, g);
}

/* c = Encrypt(r,h) */
static void
Encrypt (Fq * c, const small * r, const Fq * h)
{
  Fq hr[p];

  Rq_mult_small (hr, h, r);
  Round (c, hr);
}

/* r = Decrypt(c,(f,ginv)) */
static void
Decrypt (small * r, const Fq * c, const small * f, const small * ginv)
{
  Fq cf[p];
  Fq cf3[p];
  small e[p];
  small ev[p];
  int mask;
  int i;

  Rq_mult_small (cf, c, f);
  Rq_mult3 (cf3, cf);
  R3_fromRq (e, cf3);
  R3_mult (ev, e, ginv);

  mask = Weightw_mask (ev);	/* 0 if weight w, else -1 */
  for (i = 0; i < w; ++i)
    r[i] = ((ev[i] ^ 1) & ~mask) ^ 1;
  for (i = w; i < p; ++i)
    r[i] = ev[i] & ~mask;
}

/* ----- encoding small polynomials (including short polynomials) */

#define Small_bytes ((p+3)/4)

/* these are the only functions that rely on p mod 4 = 1 */

static void
Small_encode (unsigned char *s, const small * f)
{
  small x;
  int i;

  for (i = 0; i < p / 4; ++i)
    {
      x = *f++ + 1;
      x += (*f++ + 1) << 2;
      x += (*f++ + 1) << 4;
      x += (*f++ + 1) << 6;
      *s++ = x;
    }
  x = *f++ + 1;
  *s++ = x;
}

static void
Small_decode (small * f, const unsigned char *s)
{
  unsigned char x;
  int i;

  for (i = 0; i < p / 4; ++i)
    {
      x = *s++;
      *f++ = ((small) (x & 3)) - 1;
      x >>= 2;
      *f++ = ((small) (x & 3)) - 1;
      x >>= 2;
      *f++ = ((small) (x & 3)) - 1;
      x >>= 2;
      *f++ = ((small) (x & 3)) - 1;
    }
  x = *s++;
  *f++ = ((small) (x & 3)) - 1;
}

/* ----- encoding general polynomials */

static void
Rq_encode (unsigned char *s, const Fq * r)
{
  uint16_t R[p], M[p];
  int i;

  for (i = 0; i < p; ++i)
    R[i] = r[i] + q12;
  for (i = 0; i < p; ++i)
    M[i] = q;
  Encode (s, R, M, p);
}

static void
Rq_decode (Fq * r, const unsigned char *s)
{
  uint16_t R[p], M[p];
  int i;

  for (i = 0; i < p; ++i)
    M[i] = q;
  Decode (R, s, M, p);
  for (i = 0; i < p; ++i)
    r[i] = ((Fq) R[i]) - q12;
}

/* ----- encoding rounded polynomials */

static void
Rounded_encode (unsigned char *s, const Fq * r)
{
  uint16_t R[p], M[p];
  int i;

  for (i = 0; i < p; ++i)
    R[i] = ((r[i] + q12) * 10923) >> 15;
  for (i = 0; i < p; ++i)
    M[i] = (q + 2) / 3;
  Encode (s, R, M, p);
}

static void
Rounded_decode (Fq * r, const unsigned char *s)
{
  uint16_t R[p], M[p];
  int i;

  for (i = 0; i < p; ++i)
    M[i] = (q + 2) / 3;
  Decode (R, s, M, p);
  for (i = 0; i < p; ++i)
    r[i] = R[i] * 3 - q12;
}

/* ----- Streamlined NTRU Prime Core plus encoding */

typedef small Inputs[p];	/* passed by reference */
#define Inputs_random Short_random
#define Inputs_encode Small_encode
#define Inputs_bytes Small_bytes

#define Ciphertexts_bytes Rounded_bytes
#define SecretKeys_bytes (2*Small_bytes)
#define PublicKeys_bytes Rq_bytes

/* pk,sk = ZKeyGen() */
static void
ZKeyGen (unsigned char *pk, unsigned char *sk, void *random_ctx,
	 sntrup761_random_func * random)
{
  Fq h[p];
  small f[p], v[p];

  KeyGen (h, f, v, random_ctx, random);
  Rq_encode (pk, h);
  Small_encode (sk, f);
  sk += Small_bytes;
  Small_encode (sk, v);
}

/* C = ZEncrypt(r,pk) */
static void
ZEncrypt (unsigned char *C, const Inputs r, const unsigned char *pk)
{
  Fq h[p];
  Fq c[p];
  Rq_decode (h, pk);
  Encrypt (c, r, h);
  Rounded_encode (C, c);
}

/* r = ZDecrypt(C,sk) */
static void
ZDecrypt (Inputs r, const unsigned char *C, const unsigned char *sk)
{
  small f[p], v[p];
  Fq c[p];

  Small_decode (f, sk);
  sk += Small_bytes;
  Small_decode (v, sk);
  Rounded_decode (c, C);
  Decrypt (r, c, f, v);
}

/* ----- confirmation hash */

#define Confirm_bytes 32

/* h = HashConfirm(r,pk,cache); cache is Hash4(pk) */
static void
HashConfirm (unsigned char *h, const unsigned char *r,
	     /* const unsigned char *pk, */ const unsigned char *cache)
{
  unsigned char x[Hash_bytes * 2];
  int i;

  Hash_prefix (x, 3, r, Inputs_bytes);
  for (i = 0; i < Hash_bytes; ++i)
    x[Hash_bytes + i] = cache[i];
  Hash_prefix (h, 2, x, sizeof x);
}

/* ----- session-key hash */

/* k = HashSession(b,y,z) */
static void
HashSession (unsigned char *k, int b, const unsigned char *y,
	     const unsigned char *z)
{
  unsigned char x[Hash_bytes + Ciphertexts_bytes + Confirm_bytes];
  int i;

  Hash_prefix (x, 3, y, Inputs_bytes);
  for (i = 0; i < Ciphertexts_bytes + Confirm_bytes; ++i)
    x[Hash_bytes + i] = z[i];
  Hash_prefix (k, b, x, sizeof x);
}

/* ----- Streamlined NTRU Prime */

/* pk,sk = KEM_KeyGen() */
void
sntrup761_keypair (unsigned char *pk, unsigned char *sk, void *random_ctx,
		   sntrup761_random_func * random)
{
  int i;

  ZKeyGen (pk, sk, random_ctx, random);
  sk += SecretKeys_bytes;
  for (i = 0; i < PublicKeys_bytes; ++i)
    *sk++ = pk[i];
  random (random_ctx, Inputs_bytes, sk);
  sk += Inputs_bytes;
  Hash_prefix (sk, 4, pk, PublicKeys_bytes);
}

/* c,r_enc = Hide(r,pk,cache); cache is Hash4(pk) */
static void
Hide (unsigned char *c, unsigned char *r_enc, const Inputs r,
      const unsigned char *pk, const unsigned char *cache)
{
  Inputs_encode (r_enc, r);
  ZEncrypt (c, r, pk);
  c += Ciphertexts_bytes;
  HashConfirm (c, r_enc, cache);
}

/* c,k = Encap(pk) */
void
sntrup761_enc (unsigned char *c, unsigned char *k, const unsigned char *pk,
	       void *random_ctx, sntrup761_random_func * random)
{
  Inputs r;
  unsigned char r_enc[Inputs_bytes];
  unsigned char cache[Hash_bytes];

  Hash_prefix (cache, 4, pk, PublicKeys_bytes);
  Inputs_random (r, random_ctx, random);
  Hide (c, r_enc, r, pk, cache);
  HashSession (k, 1, r_enc, c);
}

/* 0 if matching ciphertext+confirm, else -1 */
static int
Ciphertexts_diff_mask (const unsigned char *c, const unsigned char *c2)
{
  uint16_t differentbits = 0;
  int len = Ciphertexts_bytes + Confirm_bytes;

  while (len-- > 0)
    differentbits |= (*c++) ^ (*c2++);
  return (1 & ((differentbits - 1) >> 8)) - 1;
}

/* k = Decap(c,sk) */
void
sntrup761_dec (unsigned char *k, const unsigned char *c, const unsigned char *sk)
{
  const unsigned char *pk = sk + SecretKeys_bytes;
  const unsigned char *rho = pk + PublicKeys_bytes;
  const unsigned char *cache = rho + Inputs_bytes;
  Inputs r;
  unsigned char r_enc[Inputs_bytes];
  unsigned char cnew[Ciphertexts_bytes + Confirm_bytes];
  int mask;
  int i;

  ZDecrypt (r, c, sk);
  Hide (cnew, r_enc, r, pk, cache);
  mask = Ciphertexts_diff_mask (c, cnew);
  for (i = 0; i < Inputs_bytes; ++i)
    r_enc[i] ^= mask & (r_enc[i] ^ rho[i]);
  HashSession (k, 1 + mask, r_enc, c);
}
