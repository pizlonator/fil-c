/* magma.c - GOST R 34.12-2015 (Magma) cipher implementation
 *
 * Copyright: 2017 Dmitry Eremin-Solenikov <dbaryshkov@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_NETTLE_MAGMA_SET_KEY

#include <assert.h>

#include <nettle/macros.h>
#include "nettle-write.h"
#include "magma.h"
#ifndef HAVE_NETTLE_GOST28147_SET_KEY
#include "gost28147.h"
#else
#include <nettle/gost28147.h>
#endif

void magma_set_key(struct magma_ctx *ctx, const uint8_t *key)
{
	unsigned i;

	for (i = 0; i < 8; i++, key += 4)
		ctx->key[i] = READ_UINT32(key);
}

void magma_encrypt(const struct magma_ctx *ctx, size_t length, uint8_t *dst,
		   const uint8_t *src)
{
	uint32_t block[2];

	assert(!(length % MAGMA_BLOCK_SIZE));

	while (length) {
		block[1] = READ_UINT32(src);
		src += 4;
		block[0] = READ_UINT32(src);
		src += 4;
		gost28147_encrypt_simple(ctx->key, gost28147_param_TC26_Z.sbox,
					 block, block);
		WRITE_UINT32(dst, block[1]);
		dst += 4;
		WRITE_UINT32(dst, block[0]);
		dst += 4;
		length -= MAGMA_BLOCK_SIZE;
	}
}

void magma_decrypt(const struct magma_ctx *ctx, size_t length, uint8_t *dst,
		   const uint8_t *src)
{
	uint32_t block[2];

	assert(!(length % MAGMA_BLOCK_SIZE));

	while (length) {
		block[1] = READ_UINT32(src);
		src += 4;
		block[0] = READ_UINT32(src);
		src += 4;
		gost28147_decrypt_simple(ctx->key, gost28147_param_TC26_Z.sbox,
					 block, block);
		WRITE_UINT32(dst, block[1]);
		dst += 4;
		WRITE_UINT32(dst, block[0]);
		dst += 4;
		length -= MAGMA_BLOCK_SIZE;
	}
}
#endif /* HAVE_NETTLE_MAGMA_SET_KEY */
