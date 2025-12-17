/* kuznyechik.c - GOST R 34.12-2015 (Kuznyechik) cipher implementation

   Copyright: 2017 Dmitry Eremin-Solenikov <dbaryshkov@gmail.com>

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see https://www.gnu.org/licenses/.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_NETTLE_KUZNYECHIK_SET_KEY

#include <assert.h>
#include <string.h>

#include <nettle/macros.h>
#include <nettle/memxor.h>
#include "nettle-write.h"
#include "kuznyechik.h"

#include "kuztable.h"

static void S(uint8_t *a, const uint8_t *b)
{
	a[0] = pi[b[0]];
	a[1] = pi[b[1]];
	a[2] = pi[b[2]];
	a[3] = pi[b[3]];
	a[4] = pi[b[4]];
	a[5] = pi[b[5]];
	a[6] = pi[b[6]];
	a[7] = pi[b[7]];
	a[8] = pi[b[8]];
	a[9] = pi[b[9]];
	a[10] = pi[b[10]];
	a[11] = pi[b[11]];
	a[12] = pi[b[12]];
	a[13] = pi[b[13]];
	a[14] = pi[b[14]];
	a[15] = pi[b[15]];
}

static void Sinv(uint8_t *a, const uint8_t *b)
{
	a[0] = pi_inv[b[0]];
	a[1] = pi_inv[b[1]];
	a[2] = pi_inv[b[2]];
	a[3] = pi_inv[b[3]];
	a[4] = pi_inv[b[4]];
	a[5] = pi_inv[b[5]];
	a[6] = pi_inv[b[6]];
	a[7] = pi_inv[b[7]];
	a[8] = pi_inv[b[8]];
	a[9] = pi_inv[b[9]];
	a[10] = pi_inv[b[10]];
	a[11] = pi_inv[b[11]];
	a[12] = pi_inv[b[12]];
	a[13] = pi_inv[b[13]];
	a[14] = pi_inv[b[14]];
	a[15] = pi_inv[b[15]];
}

static void Linv(uint8_t *a, const uint8_t *b)
{
	memcpy(a, &kuz_table_inv[0][b[0] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[1][b[1] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[2][b[2] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[3][b[3] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[4][b[4] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[5][b[5] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[6][b[6] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[7][b[7] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[8][b[8] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[9][b[9] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[10][b[10] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[11][b[11] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[12][b[12] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[13][b[13] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[14][b[14] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(a, &kuz_table_inv[15][b[15] * 16], KUZNYECHIK_BLOCK_SIZE);
}

static void LSX(uint8_t *a, const uint8_t *b, const uint8_t *c)
{
	uint8_t t[16];

	/* https://github.com/llvm/llvm-project/issues/53518 */
#if defined(__clang_analyzer__) && \
	(defined(__clang_major__) && __clang_major__ == 13)
	assert(0);
#else
	memcpy(t, &kuz_table[0][(b[0] ^ c[0]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[1][(b[1] ^ c[1]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[2][(b[2] ^ c[2]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[3][(b[3] ^ c[3]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[4][(b[4] ^ c[4]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[5][(b[5] ^ c[5]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[6][(b[6] ^ c[6]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[7][(b[7] ^ c[7]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[8][(b[8] ^ c[8]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[9][(b[9] ^ c[9]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[10][(b[10] ^ c[10]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[11][(b[11] ^ c[11]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[12][(b[12] ^ c[12]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[13][(b[13] ^ c[13]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table[14][(b[14] ^ c[14]) * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor3(a, t, &kuz_table[15][(b[15] ^ c[15]) * 16],
		KUZNYECHIK_BLOCK_SIZE);
#endif
}

static void XLiSi(uint8_t *a, const uint8_t *b, const uint8_t *c)
{
	uint8_t t[16];

	memcpy(t, &kuz_table_inv_LS[0][b[0] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[1][b[1] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[2][b[2] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[3][b[3] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[4][b[4] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[5][b[5] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[6][b[6] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[7][b[7] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[8][b[8] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[9][b[9] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[10][b[10] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[11][b[11] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[12][b[12] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[13][b[13] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[14][b[14] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor(t, &kuz_table_inv_LS[15][b[15] * 16], KUZNYECHIK_BLOCK_SIZE);
	memxor3(a, t, c, 16);
}

static void subkey(uint8_t *out, const uint8_t *key, unsigned i)
{
	uint8_t test[16];

	LSX(test, key + 0, kuz_key_table[i + 0]);
	memxor3(out + 16, test, key + 16, 16);
	LSX(test, out + 16, kuz_key_table[i + 1]);
	memxor3(out + 0, test, key + 0, 16);
	LSX(test, out + 0, kuz_key_table[i + 2]);
	memxor(out + 16, test, 16);
	LSX(test, out + 16, kuz_key_table[i + 3]);
	memxor(out + 0, test, 16);
	LSX(test, out + 0, kuz_key_table[i + 4]);
	memxor(out + 16, test, 16);
	LSX(test, out + 16, kuz_key_table[i + 5]);
	memxor(out + 0, test, 16);
	LSX(test, out + 0, kuz_key_table[i + 6]);
	memxor(out + 16, test, 16);
	LSX(test, out + 16, kuz_key_table[i + 7]);
	memxor(out + 0, test, 16);
}

void kuznyechik_set_key(struct kuznyechik_ctx *ctx, const uint8_t *key)
{
	unsigned i;

	memcpy(ctx->key, key, 32);
	subkey(ctx->key + 32, ctx->key, 0);
	subkey(ctx->key + 64, ctx->key + 32, 8);
	subkey(ctx->key + 96, ctx->key + 64, 16);
	subkey(ctx->key + 128, ctx->key + 96, 24);
	for (i = 0; i < 10; i++)
		Linv(ctx->dekey + 16 * i, ctx->key + 16 * i);
}

void kuznyechik_encrypt(const struct kuznyechik_ctx *ctx, size_t length,
			uint8_t *dst, const uint8_t *src)
{
	uint8_t temp[KUZNYECHIK_BLOCK_SIZE];

	assert(!(length % KUZNYECHIK_BLOCK_SIZE));

	while (length) {
		LSX(temp, ctx->key + 16 * 0, src);
		LSX(temp, ctx->key + 16 * 1, temp);
		LSX(temp, ctx->key + 16 * 2, temp);
		LSX(temp, ctx->key + 16 * 3, temp);
		LSX(temp, ctx->key + 16 * 4, temp);
		LSX(temp, ctx->key + 16 * 5, temp);
		LSX(temp, ctx->key + 16 * 6, temp);
		LSX(temp, ctx->key + 16 * 7, temp);
		LSX(temp, ctx->key + 16 * 8, temp);
		memxor3(dst, ctx->key + 16 * 9, temp, 16);
		src += KUZNYECHIK_BLOCK_SIZE;
		dst += KUZNYECHIK_BLOCK_SIZE;
		length -= KUZNYECHIK_BLOCK_SIZE;
	}
}

void kuznyechik_decrypt(const struct kuznyechik_ctx *ctx, size_t length,
			uint8_t *dst, const uint8_t *src)
{
	uint8_t temp[KUZNYECHIK_BLOCK_SIZE];

	assert(!(length % KUZNYECHIK_BLOCK_SIZE));

	while (length) {
		S(temp, src);
		XLiSi(temp, temp, ctx->dekey + 16 * 9);
		XLiSi(temp, temp, ctx->dekey + 16 * 8);
		XLiSi(temp, temp, ctx->dekey + 16 * 7);
		XLiSi(temp, temp, ctx->dekey + 16 * 6);
		XLiSi(temp, temp, ctx->dekey + 16 * 5);
		XLiSi(temp, temp, ctx->dekey + 16 * 4);
		XLiSi(temp, temp, ctx->dekey + 16 * 3);
		XLiSi(temp, temp, ctx->dekey + 16 * 2);
		XLiSi(temp, temp, ctx->dekey + 16 * 1);
		Sinv(dst, temp);
		memxor(dst, ctx->key + 16 * 0, 16);
		src += KUZNYECHIK_BLOCK_SIZE;
		dst += KUZNYECHIK_BLOCK_SIZE;
		length -= KUZNYECHIK_BLOCK_SIZE;
	}
}
#endif /* HAVE_NETTLE_KUZNYECHIK_SET_KEY */
