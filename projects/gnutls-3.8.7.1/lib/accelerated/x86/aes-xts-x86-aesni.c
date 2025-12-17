/*
 * Copyright (C) 2011-2012 Free Software Foundation, Inc.
 * Copyright (C) 2020 Red Hat, Inc.
 *
 * Authors: Nikos Mavrogiannopoulos, Anderson Toshiyuki Sasaki
 *
 * This file is part of GnuTLS.
 *
 * The GnuTLS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 *
 */

/*
 * The following code wraps the CRYPTOGAMS implementation of the AES-XTS cipher
 * using Intel's AES instruction set.
 */

#include "errors.h"
#include "gnutls_int.h"
#include "fips.h"
#include <gnutls/crypto.h>
#include "aes-x86.h"
#include "x86-common.h"

struct x86_aes_xts_ctx {
	AES_KEY block_key;
	AES_KEY tweak_key;
	uint8_t iv[16];
	int enc;
};

static int x86_aes_xts_cipher_init(gnutls_cipher_algorithm_t algorithm,
				   void **_ctx, int enc)
{
	if (algorithm != GNUTLS_CIPHER_AES_128_XTS &&
	    algorithm != GNUTLS_CIPHER_AES_256_XTS)
		return GNUTLS_E_INVALID_REQUEST;

	*_ctx = gnutls_calloc(1, sizeof(struct x86_aes_xts_ctx));
	if (*_ctx == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	((struct x86_aes_xts_ctx *)(*_ctx))->enc = enc;

	return 0;
}

static int x86_aes_xts_cipher_setkey(void *_ctx, const void *userkey,
				     size_t keysize)
{
	struct x86_aes_xts_ctx *ctx = _ctx;
	int ret;
	size_t keybits;
	const uint8_t *key = userkey;

	if ((keysize != 32) && (keysize != 64))
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	/* Check key block according to FIPS-140-2 IG A.9 */
	if (_gnutls_fips_mode_enabled()) {
		if (gnutls_memcmp(key, key + (keysize / 2), keysize / 2) == 0) {
			return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
		}
	}

	/* Size in bits of each half for block and tweak (=keysize * 8 / 2) */
	keybits = keysize * 4;

	if (ctx->enc)
		ret = aesni_set_encrypt_key(key, keybits,
					    ALIGN16(&ctx->block_key));
	else
		ret = aesni_set_decrypt_key(key, keybits,
					    ALIGN16(&ctx->block_key));

	if (ret != 0)
		return gnutls_assert_val(GNUTLS_E_ENCRYPTION_FAILED);

	ret = aesni_set_encrypt_key(key + (keysize / 2), keybits,
				    ALIGN16(&ctx->tweak_key));
	if (ret != 0)
		return gnutls_assert_val(GNUTLS_E_ENCRYPTION_FAILED);

	return 0;
}

static int x86_aes_xts_setiv(void *_ctx, const void *iv, size_t iv_size)
{
	struct x86_aes_xts_ctx *ctx = _ctx;

	if (iv_size != 16)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	memcpy(ctx->iv, iv, 16);
	return 0;
}

static int x86_aes_xts_encrypt(void *_ctx, const void *src, size_t src_size,
			       void *dst, size_t dst_size)
{
	struct x86_aes_xts_ctx *ctx = _ctx;

	if (unlikely(dst_size < src_size))
		return gnutls_assert_val(GNUTLS_E_SHORT_MEMORY_BUFFER);

	if (src_size < 16)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	aesni_xts_encrypt(src, dst, src_size, ALIGN16(&ctx->block_key),
			  ALIGN16(&ctx->tweak_key), ctx->iv);
	return 0;
}

static int x86_aes_xts_decrypt(void *_ctx, const void *src, size_t src_size,
			       void *dst, size_t dst_size)
{
	struct x86_aes_xts_ctx *ctx = _ctx;

	if (unlikely(dst_size < src_size))
		return gnutls_assert_val(GNUTLS_E_SHORT_MEMORY_BUFFER);

	if (src_size < 16)
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	aesni_xts_decrypt(src, dst, src_size, ALIGN16(&ctx->block_key),
			  ALIGN16(&ctx->tweak_key), ctx->iv);
	return 0;
}

static void x86_aes_xts_deinit(void *_ctx)
{
	struct x86_aes_xts_ctx *ctx = _ctx;

	zeroize_temp_key(ctx, sizeof(*ctx));
	gnutls_free(ctx);
}

const gnutls_crypto_cipher_st _gnutls_aes_xts_x86_aesni = {
	.init = x86_aes_xts_cipher_init,
	.setkey = x86_aes_xts_cipher_setkey,
	.setiv = x86_aes_xts_setiv,
	.encrypt = x86_aes_xts_encrypt,
	.decrypt = x86_aes_xts_decrypt,
	.deinit = x86_aes_xts_deinit,
};
