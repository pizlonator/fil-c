/*
 * Copyright (C) 2018 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "tls.h"

typedef uint8_t  byte;
typedef uint16_t word16;
typedef uint32_t word32;
#define XMEMSET  memset

#define F25519_SIZE CURVE25519_KEYSIZE

/* The code below is taken from wolfssl-3.15.3/wolfcrypt/src/fe_low_mem.c
 * Header comment is kept intact:
 */

/* fe_low_mem.c
 *
 * Copyright (C) 2006-2017 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */


/* Based from Daniel Beer's public domain work. */

#if 0 //UNUSED
static void fprime_copy(byte *x, const byte *a)
{
	memcpy(x, a, F25519_SIZE);
}
#endif

static void lm_copy(byte* x, const byte* a)
{
	memcpy(x, a, F25519_SIZE);
}

#if 0 //UNUSED
static void fprime_select(byte *dst, const byte *zero, const byte *one, byte condition)
{
	const byte mask = -condition;
	int i;

	for (i = 0; i < F25519_SIZE; i++)
		dst[i] = zero[i] ^ (mask & (one[i] ^ zero[i]));
}
#endif

#if 0 /* constant-time */
static void fe_select(byte *dst,
		const byte *src,
		byte condition)
{
	const byte mask = -condition;
	int i;

	for (i = 0; i < F25519_SIZE; i++)
		dst[i] = dst[i] ^ (mask & (src[i] ^ dst[i]));
}
#else
# define fe_select(dst, src, condition) do { \
	if (condition) lm_copy(dst, src); \
} while (0)
#endif

#if 0 //UNUSED
static void raw_add(byte *x, const byte *p)
{
	word16 c = 0;
	int i;

	for (i = 0; i < F25519_SIZE; i++) {
		c += ((word16)x[i]) + ((word16)p[i]);
		x[i] = (byte)c;
		c >>= 8;
	}
}
#endif

#if 0 //UNUSED
static void raw_try_sub(byte *x, const byte *p)
{
	byte minusp[F25519_SIZE];
	word16 c = 0;
	int i;

	for (i = 0; i < F25519_SIZE; i++) {
		c = ((word16)x[i]) - ((word16)p[i]) - c;
		minusp[i] = (byte)c;
		c = (c >> 8) & 1;
	}

	fprime_select(x, minusp, x, (byte)c);
}
#endif

#if 0 //UNUSED
static int prime_msb(const byte *p)
{
	int i;
	byte x;
	int shift = 1;
	int z     = F25519_SIZE - 1;

	/*
	    Test for any hot bits.
	    As soon as one instance is encountered set shift to 0.
	 */
	for (i = F25519_SIZE - 1; i >= 0; i--) {
		shift &= ((shift ^ ((-p[i] | p[i]) >> 7)) & 1);
		z -= shift;
	}
	x = p[z];
	z <<= 3;
	shift = 1;
	for (i = 0; i < 8; i++) {
		shift &= ((-(x >> i) | (x >> i)) >> (7 - i) & 1);
		z += shift;
	}

	return z - 1;
}
#endif

#if 0 //UNUSED
static void fprime_add(byte *r, const byte *a, const byte *modulus)
{
	raw_add(r, a);
	raw_try_sub(r, modulus);
}
#endif

#if 0 //UNUSED
static void fprime_sub(byte *r, const byte *a, const byte *modulus)
{
	raw_add(r, modulus);
	raw_try_sub(r, a);
	raw_try_sub(r, modulus);
}
#endif

#if 0 //UNUSED
static void fprime_mul(byte *r, const byte *a, const byte *b,
		const byte *modulus)
{
	word16 c = 0;
	int i,j;

	XMEMSET(r, 0, F25519_SIZE);

	for (i = prime_msb(modulus); i >= 0; i--) {
		const byte bit = (b[i >> 3] >> (i & 7)) & 1;
		byte plusa[F25519_SIZE];

		for (j = 0; j < F25519_SIZE; j++) {
			c |= ((word16)r[j]) << 1;
			r[j] = (byte)c;
			c >>= 8;
		}
		raw_try_sub(r, modulus);

		fprime_copy(plusa, r);
		fprime_add(plusa, a, modulus);

		fprime_select(r, r, plusa, bit);
	}
}
#endif

#if 0 //UNUSED
static void fe_load(byte *x, word32 c)
{
	int i;

	for (i = 0; i < sizeof(c); i++) {
		x[i] = c;
		c >>= 8;
	}

	for (; i < F25519_SIZE; i++)
		x[i] = 0;
}
#endif

static void fe_reduce(byte *x, word32 c)
{
	int i;

	/* Reduce using 2^255 = 19 mod p */
	x[31] = c & 127;
	c = (c >> 7) * 19;

	for (i = 0; i < F25519_SIZE; i++) {
		c += x[i];
		x[i] = (byte)c;
		c >>= 8;
	}
}

static void fe_normalize(byte *x)
{
	byte minusp[F25519_SIZE];
	unsigned c;
	int i;

	/* Reduce using 2^255 = 19 mod p */
	fe_reduce(x, x[31]);

	/* The number is now less than 2^255 + 18, and therefore less than
	 * 2p. Try subtracting p, and conditionally load the subtracted
	 * value if underflow did not occur.
	 */
	c = 19;

	for (i = 0; i < F25519_SIZE - 1; i++) {
		c += x[i];
		minusp[i] = (byte)c;
		c >>= 8;
	}

	c += ((unsigned)x[i]) - 128;
	minusp[31] = (byte)c;

	/* Load x-p if no underflow */
	fe_select(x, minusp, !(c & (1<<15)));
}

static void lm_add(byte* r, const byte* a, const byte* b)
{
	unsigned c = 0;
	int i;

	/* Add */
	for (i = 0; i < F25519_SIZE; i++) {
		c >>= 8;
		c += ((unsigned)a[i]) + ((unsigned)b[i]);
		r[i] = (byte)c;
	}

	/* Reduce with 2^255 = 19 mod p */
	fe_reduce(r, c);
}

static void lm_sub(byte* r, const byte* a, const byte* b)
{
	word32 c = 0;
	int i;

	/* Calculate a + 2p - b, to avoid underflow */
	c = 218;
	for (i = 0; i < F25519_SIZE - 1; i++) {
		c += 65280 + ((word32)a[i]) - ((word32)b[i]);
		r[i] = c;
		c >>= 8;
	}

	c += ((word32)a[31]) - ((word32)b[31]);

	fe_reduce(r, c);
}

#if 0 //UNUSED
static void lm_neg(byte* r, const byte* a)
{
	word32 c = 0;
	int i;

	/* Calculate 2p - a, to avoid underflow */
	c = 218;
	for (i = 0; i < F25519_SIZE - 1; i++) {
		c += 65280 - ((word32)a[i]);
		r[i] = c;
		c >>= 8;
	}

	c -= ((word32)a[31]);

	fe_reduce(r, c);
}
#endif

static void fe_mul__distinct(byte *r, const byte *a, const byte *b)
{
	word32 c = 0;
	int i;

	for (i = 0; i < F25519_SIZE; i++) {
		int j;

		c >>= 8;
		for (j = 0; j <= i; j++)
			c += ((word32)a[j]) * ((word32)b[i - j]);

		for (; j < F25519_SIZE; j++)
			c += ((word32)a[j]) *
				((word32)b[i + F25519_SIZE - j]) * 38;

		r[i] = c;
	}

	fe_reduce(r, c);
}

#if 0 //UNUSED
static void lm_mul(byte *r, const byte* a, const byte *b)
{
	byte tmp[F25519_SIZE];

	fe_mul__distinct(tmp, a, b);
	lm_copy(r, tmp);
}
#endif

static void fe_mul_c(byte *r, const byte *a, word32 b)
{
	word32 c = 0;
	int i;

	for (i = 0; i < F25519_SIZE; i++) {
		c >>= 8;
		c += b * ((word32)a[i]);
		r[i] = c;
	}

	fe_reduce(r, c);
}

static void fe_inv__distinct(byte *r, const byte *x)
{
	byte s[F25519_SIZE];
	int i;

	/* This is a prime field, so by Fermat's little theorem:
	 *
	 *     x^(p-1) = 1 mod p
	 *
	 * Therefore, raise to (p-2) = 2^255-21 to get a multiplicative
	 * inverse.
	 *
	 * This is a 255-bit binary number with the digits:
	 *
	 *     11111111... 01011
	 *
	 * We compute the result by the usual binary chain, but
	 * alternate between keeping the accumulator in r and s, so as
	 * to avoid copying temporaries.
	 */

	lm_copy(r, x);

	/* 1, 1 x 249 */
	for (i = 0; i < 249; i++) {
		fe_mul__distinct(s, r, r);
		fe_mul__distinct(r, s, x);
	}

	/* 0 */
	fe_mul__distinct(s, r, r);

	/* 1 */
	fe_mul__distinct(r, s, s);
	fe_mul__distinct(s, r, x);

	/* 0 */
	fe_mul__distinct(r, s, s);

	/* 1, 1 */
	for (i = 0; i < 2; i++) {
		fe_mul__distinct(s, r, r);
		fe_mul__distinct(r, s, x);
	}
}

#if 0 //UNUSED
static void lm_invert(byte *r, const byte *x)
{
	byte tmp[F25519_SIZE];

	fe_inv__distinct(tmp, x);
	lm_copy(r, tmp);
}
#endif

#if 0 //UNUSED
/* Raise x to the power of (p-5)/8 = 2^252-3, using s for temporary
 * storage.
 */
static void exp2523(byte *r, const byte *x, byte *s)
{
	int i;

	/* This number is a 252-bit number with the binary expansion:
	 *
	 *     111111... 01
	 */

	lm_copy(s, x);

	/* 1, 1 x 249 */
	for (i = 0; i < 249; i++) {
		fe_mul__distinct(r, s, s);
		fe_mul__distinct(s, r, x);
	}

	/* 0 */
	fe_mul__distinct(r, s, s);

	/* 1 */
	fe_mul__distinct(s, r, r);
	fe_mul__distinct(r, s, x);
}
#endif

#if 0 //UNUSED
static void fe_sqrt(byte *r, const byte *a)
{
	byte v[F25519_SIZE];
	byte i[F25519_SIZE];
	byte x[F25519_SIZE];
	byte y[F25519_SIZE];

	/* v = (2a)^((p-5)/8) [x = 2a] */
	fe_mul_c(x, a, 2);
	exp2523(v, x, y);

	/* i = 2av^2 - 1 */
	fe_mul__distinct(y, v, v);
	fe_mul__distinct(i, x, y);
	fe_load(y, 1);
	lm_sub(i, i, y);

	/* r = avi */
	fe_mul__distinct(x, v, a);
	fe_mul__distinct(r, x, i);
}
#endif

/* Differential addition */
static void xc_diffadd(byte *x5, byte *z5,
		const byte *x1, const byte *z1,
		const byte *x2, const byte *z2,
		const byte *x3, const byte *z3)
{
	/* Explicit formulas database: dbl-1987-m3
	 *
	 * source 1987 Montgomery "Speeding the Pollard and elliptic curve
	 *   methods of factorization", page 261, fifth display, plus
	 *   common-subexpression elimination
	 * compute A = X2+Z2
	 * compute B = X2-Z2
	 * compute C = X3+Z3
	 * compute D = X3-Z3
	 * compute DA = D A
	 * compute CB = C B
	 * compute X5 = Z1(DA+CB)^2
	 * compute Z5 = X1(DA-CB)^2
	 */
	byte da[F25519_SIZE];
	byte cb[F25519_SIZE];
	byte a[F25519_SIZE];
	byte b[F25519_SIZE];

	lm_add(a, x2, z2);
	lm_sub(b, x3, z3); /* D */
	fe_mul__distinct(da, a, b);

	lm_sub(b, x2, z2);
	lm_add(a, x3, z3); /* C */
	fe_mul__distinct(cb, a, b);

	lm_add(a, da, cb);
	fe_mul__distinct(b, a, a);
	fe_mul__distinct(x5, z1, b);

	lm_sub(a, da, cb);
	fe_mul__distinct(b, a, a);
	fe_mul__distinct(z5, x1, b);
}

/* Double an X-coordinate */
static void xc_double(byte *x3, byte *z3,
		const byte *x1, const byte *z1)
{
	/* Explicit formulas database: dbl-1987-m
	 *
	 * source 1987 Montgomery "Speeding the Pollard and elliptic
	 *   curve methods of factorization", page 261, fourth display
	 * compute X3 = (X1^2-Z1^2)^2
	 * compute Z3 = 4 X1 Z1 (X1^2 + a X1 Z1 + Z1^2)
	 */
	byte x1sq[F25519_SIZE];
	byte z1sq[F25519_SIZE];
	byte x1z1[F25519_SIZE];
	byte a[F25519_SIZE];

	fe_mul__distinct(x1sq, x1, x1);
	fe_mul__distinct(z1sq, z1, z1);
	fe_mul__distinct(x1z1, x1, z1);

	lm_sub(a, x1sq, z1sq);
	fe_mul__distinct(x3, a, a);

	fe_mul_c(a, x1z1, 486662);
	lm_add(a, x1sq, a);
	lm_add(a, z1sq, a);
	fe_mul__distinct(x1sq, x1z1, a);
	fe_mul_c(z3, x1sq, 4);
}

static void curve25519(byte *result, const byte *e, const byte *q)
{
	int i;

	struct Z {
		/* for bbox's special case of q == NULL meaning "use basepoint" */
		/*static const*/ uint8_t basepoint9[CURVE25519_KEYSIZE]; // = {9};

		/* from wolfssl-3.15.3/wolfssl/wolfcrypt/fe_operations.h */
		/*static const*/ byte f25519_one[F25519_SIZE]; // = {1};

		/* Predecessor: P_(m-1) */
		byte xm1[F25519_SIZE]; // = {1};
		byte zm1[F25519_SIZE]; // = {0};
		/* Current point: P_m */
		byte xm[F25519_SIZE];
		byte zm[F25519_SIZE]; // = {1};
		/* Temporaries */
		byte xms[F25519_SIZE];
		byte zms[F25519_SIZE];
	} z;
	uint8_t *XM1 = (uint8_t*)&z + offsetof(struct Z,xm1); // gcc 11.0.0 workaround
#define basepoint9 z.basepoint9
#define f25519_one z.f25519_one
#define xm1        z.xm1
#define zm1        z.zm1
#define xm         z.xm
#define zm         z.zm
#define xms        z.xms
#define zms        z.zms
	memset(&z, 0, sizeof(z));
	f25519_one[0] = 1;
	zm[0] = 1;
	xm1[0] = 1;

	if (!q) {
		basepoint9[0] = 9;
		q = basepoint9;
	}

	/* Note: bit 254 is assumed to be 1 */
	lm_copy(xm, q);

	for (i = 253; i >= 0; i--) {
		const int bit = (e[i >> 3] >> (i & 7)) & 1;
//		byte xms[F25519_SIZE];
//		byte zms[F25519_SIZE];

		/* From P_m and P_(m-1), compute P_(2m) and P_(2m-1) */
		xc_diffadd(xm1, zm1, q, f25519_one, xm, zm, xm1, zm1);
		xc_double(xm, zm, xm, zm);

		/* Compute P_(2m+1) */
		xc_diffadd(xms, zms, xm1, zm1, xm, zm, q, f25519_one);

		/* Select:
		 *   bit = 1 --> (P_(2m+1), P_(2m))
		 *   bit = 0 --> (P_(2m), P_(2m-1))
		 */
#if 0
		fe_select(xm1, xm, bit);
		fe_select(zm1, zm, bit);
		fe_select(xm, xms, bit);
		fe_select(zm, zms, bit);
#else
// same as above in about 50 bytes smaller code, but
// requires that in-memory order is exactly xm1,zm1,xm,zm,xms,zms
		if (bit) {
			//memcpy(xm1, xm, 4 * F25519_SIZE);
			//^^^ gcc 11.0.0 warns of overlapping memcpy
			//memmove(xm1, xm, 4 * F25519_SIZE);
			//^^^ gcc 11.0.0 warns of out-of-bounds access to xm1[]
			memmove(XM1, XM1 + 2 * F25519_SIZE, 4 * F25519_SIZE);
		}
#endif
	}

	/* Freeze out of projective coordinates */
	fe_inv__distinct(zm1, zm);
	fe_mul__distinct(result, zm1, xm);
	fe_normalize(result);
}

/* interface to bbox's TLS code: */

void FAST_FUNC curve_x25519_compute_pubkey_and_premaster(
		uint8_t *pubkey, uint8_t *premaster,
		const uint8_t *peerkey32)
{
	uint8_t privkey[CURVE25519_KEYSIZE]; //[32]

	/* Generate random private key, see RFC 7748 */
	tls_get_random(privkey, sizeof(privkey));
	privkey[0] &= 0xf8;
	privkey[CURVE25519_KEYSIZE-1] = ((privkey[CURVE25519_KEYSIZE-1] & 0x7f) | 0x40);

	/* Compute public key */
	curve25519(pubkey, privkey, NULL /* "use base point of x25519" */);

	/* Compute premaster using peer's public key */
	curve25519(premaster, privkey, peerkey32);
}
