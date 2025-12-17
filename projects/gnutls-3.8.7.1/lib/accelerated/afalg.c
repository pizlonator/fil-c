/*
 * Copyright (C) 2017 Stephan Mueller <smueller@chronox.de>
 *
 * Author: Stephan Mueller
 *
 * This code is free software; you can redistribute it and/or
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "accelerated/afalg.h"

#ifdef ENABLE_AFALG

#include "errors.h"
#include "gnutls_int.h"

#include <gnutls/crypto.h>
#include <kcapi.h>
#include <limits.h>
#include "malloca.h"

/************************ Symmetric cipher algorithms ************************/

struct kcapi_ctx {
	struct kcapi_handle *handle;
	int enc;
	uint8_t iv[MAX_CIPHER_IV_SIZE];
};

static const char *gnutls_cipher_map[] = {
	[GNUTLS_CIPHER_AES_128_CBC] = "cbc(aes)",
	[GNUTLS_CIPHER_AES_192_CBC] = "cbc(aes)",
	[GNUTLS_CIPHER_AES_256_CBC] = "cbc(aes)",
	[GNUTLS_CIPHER_3DES_CBC] = "cbc(des3_ede)",
	[GNUTLS_CIPHER_CAMELLIA_128_CBC] = "cbc(camellia)",
	[GNUTLS_CIPHER_CAMELLIA_192_CBC] = "cbc(camellia)",
	[GNUTLS_CIPHER_CAMELLIA_256_CBC] = "cbc(camellia)",
	[GNUTLS_CIPHER_SALSA20_256] = "salsa20",
	[GNUTLS_CIPHER_AES_128_XTS] = "xts(aes)",
	[GNUTLS_CIPHER_AES_256_XTS] = "xts(aes)",
};

static int afalg_cipher_init(gnutls_cipher_algorithm_t algorithm, void **_ctx,
			     int enc)
{
	struct kcapi_handle *handle;
	struct kcapi_ctx *ctx;

	if (kcapi_cipher_init(&handle, gnutls_cipher_map[algorithm], 0) < 0) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	if (unlikely(kcapi_cipher_ivsize(handle) > MAX_CIPHER_IV_SIZE)) {
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	ctx = gnutls_malloc(sizeof(struct kcapi_ctx));
	if (ctx == NULL) {
		gnutls_assert();
		kcapi_cipher_destroy(handle);
		return GNUTLS_E_MEMORY_ERROR;
	}

	ctx->handle = handle;
	ctx->enc = enc;
	*_ctx = ctx;
	return 0;
}

static int afalg_cipher_setkey(void *_ctx, const void *key, size_t keysize)
{
	struct kcapi_ctx *ctx = _ctx;

	if (kcapi_cipher_setkey(ctx->handle, key, keysize) < 0) {
		gnutls_assert();
		return GNUTLS_E_ENCRYPTION_FAILED;
	}

	return 0;
}

static int afalg_cipher_setiv(void *_ctx, const void *iv, size_t iv_size)
{
	struct kcapi_ctx *ctx = _ctx;

	if (iv_size > kcapi_cipher_ivsize(ctx->handle))
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);

	memcpy(ctx->iv, iv, iv_size);
	if (ctx->enc) {
		if (kcapi_cipher_stream_init_enc(ctx->handle, ctx->iv, NULL,
						 0) < 0) {
			gnutls_assert();
			return GNUTLS_E_ENCRYPTION_FAILED;
		}
	} else {
		if (kcapi_cipher_stream_init_dec(ctx->handle, ctx->iv, NULL,
						 0) < 0) {
			gnutls_assert();
			return GNUTLS_E_ENCRYPTION_FAILED;
		}
	}

	return 0;
}

static int afalg_cipher_encrypt(void *_ctx, const void *src, size_t src_size,
				void *dst, size_t dst_size)
{
	struct kcapi_ctx *ctx = _ctx;
	struct iovec iov;

	iov.iov_base = (void *)src;
	iov.iov_len = src_size;

	if (unlikely(src_size % kcapi_cipher_blocksize(ctx->handle))) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	if (kcapi_cipher_stream_update(ctx->handle, &iov, 1) < 0) {
		return gnutls_assert_val(GNUTLS_E_ENCRYPTION_FAILED);
	}

	if (unlikely(dst_size < src_size))
		return gnutls_assert_val(GNUTLS_E_SHORT_MEMORY_BUFFER);

	iov.iov_base = (void *)dst;
	iov.iov_len = src_size;

	if (kcapi_cipher_stream_op(ctx->handle, &iov, 1) < 0) {
		gnutls_assert();
		return GNUTLS_E_ENCRYPTION_FAILED;
	}

	return 0;
}

static int afalg_cipher_decrypt(void *_ctx, const void *src, size_t src_size,
				void *dst, size_t dst_size)
{
	struct kcapi_ctx *ctx = _ctx;
	struct iovec iov;

	iov.iov_base = (void *)src;
	iov.iov_len = src_size;

	if (unlikely(src_size % kcapi_cipher_blocksize(ctx->handle))) {
		return gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
	}

	if (kcapi_cipher_stream_update(ctx->handle, &iov, 1) < 0) {
		return gnutls_assert_val(GNUTLS_E_ENCRYPTION_FAILED);
	}

	if (unlikely(dst_size < src_size))
		return gnutls_assert_val(GNUTLS_E_SHORT_MEMORY_BUFFER);

	iov.iov_base = (void *)dst;
	iov.iov_len = src_size;

	if (kcapi_cipher_stream_op(ctx->handle, &iov, 1) < 0) {
		gnutls_assert();
		return GNUTLS_E_ENCRYPTION_FAILED;
	}

	return 0;
}

static void afalg_cipher_deinit(void *_ctx)
{
	struct kcapi_ctx *ctx = _ctx;

	kcapi_cipher_destroy(ctx->handle);
	gnutls_free(ctx);
}

static const gnutls_crypto_cipher_st afalg_cipher_struct = {
	.init = afalg_cipher_init,
	.setkey = afalg_cipher_setkey,
	.setiv = afalg_cipher_setiv,
	.encrypt = afalg_cipher_encrypt,
	.decrypt = afalg_cipher_decrypt,
	.deinit = afalg_cipher_deinit,
};

static int afalg_cipher_register(void)
{
	unsigned int i;
	int ret = 0;

	for (i = 0;
	     i < sizeof(gnutls_cipher_map) / sizeof(gnutls_cipher_map[0]);
	     i++) {
		struct kcapi_handle *handle;

		if (gnutls_cipher_map[i] == 0)
			continue;

		/* Check whether cipher is available. */
		if (kcapi_cipher_init(&handle, gnutls_cipher_map[i], 0))
			continue;

		kcapi_cipher_destroy(handle);

		_gnutls_debug_log("afalg: registering: %s\n",
				  gnutls_cipher_get_name(i));
		ret = gnutls_crypto_single_cipher_register(
			i, 90, &afalg_cipher_struct, 0);
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}
	}

	return ret;
}

/************************ Symmetric cipher algorithms ************************/

struct kcapi_aead_ctx {
	struct kcapi_handle *handle;
	int taglen_set;
	int ccm;
};

static const char *gnutls_aead_map[] = {
	[GNUTLS_CIPHER_CAMELLIA_128_GCM] = "gcm(camellia)",
	[GNUTLS_CIPHER_CAMELLIA_256_GCM] = "gcm(camellia)",
	[GNUTLS_CIPHER_AES_128_CCM] = "ccm(aes)",
	[GNUTLS_CIPHER_AES_256_CCM] = "ccm(aes)",
	[GNUTLS_CIPHER_AES_128_GCM] = "gcm(aes)",
	[GNUTLS_CIPHER_AES_256_GCM] = "gcm(aes)",
};

static void afalg_aead_deinit(void *_ctx)
{
	struct kcapi_aead_ctx *ctx = _ctx;

	kcapi_aead_destroy(ctx->handle);
	gnutls_free(ctx);
}

static int afalg_aead_init(gnutls_cipher_algorithm_t algorithm, void **_ctx,
			   int enc)
{
	struct kcapi_handle *handle;
	struct kcapi_aead_ctx *ctx;

	if (kcapi_aead_init(&handle, gnutls_aead_map[algorithm], 0) < 0) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	ctx = gnutls_malloc(sizeof(struct kcapi_aead_ctx));
	if (ctx == NULL) {
		gnutls_assert();
		kcapi_aead_destroy(handle);
		return GNUTLS_E_MEMORY_ERROR;
	}

	switch (algorithm) {
	case GNUTLS_CIPHER_AES_128_CCM:
	case GNUTLS_CIPHER_AES_256_CCM:
		ctx->ccm = 1;
		break;
	default:
		ctx->ccm = 0;
	}
	ctx->handle = handle;
	*_ctx = ctx;

	return 0;
}

static int afalg_aead_setkey(void *_ctx, const void *key, size_t keysize)
{
	struct kcapi_aead_ctx *ctx = _ctx;

	if (kcapi_aead_setkey(ctx->handle, key, keysize) < 0) {
		gnutls_assert();
		return GNUTLS_E_ENCRYPTION_FAILED;
	}

	return 0;
}

static int afalg_aead_decrypt(void *_ctx, const void *nonce, size_t nonce_size,
			      const void *auth, size_t auth_size,
			      size_t tag_size, const void *encr,
			      size_t encr_size, void *plain, size_t plain_size)
{
	int ret = 0;
	struct kcapi_aead_ctx *ctx = _ctx;
	struct iovec iov[2];
	uint8_t *authtmp = malloca(auth_size);
	if (authtmp == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}
	if (encr_size < tag_size) {
		gnutls_assert();
		ret = GNUTLS_E_DECRYPTION_FAILED;
		goto end;
	}

	if (unlikely(plain_size < encr_size - tag_size)) {
		gnutls_assert();
		ret = GNUTLS_E_SHORT_MEMORY_BUFFER;
		goto end;
	}

	/* Init stream once. */
	if (!ctx->taglen_set) {
		ctx->taglen_set = 1;
		if (kcapi_aead_settaglen(ctx->handle, tag_size) < 0) {
			gnutls_assert();
			ret = GNUTLS_E_DECRYPTION_FAILED;
			goto end;
		}
	}

	kcapi_aead_setassoclen(ctx->handle, auth_size);

	/* CCM nonce to IV conversion */
	if (ctx->ccm) {
		uint8_t *ccm_iv = NULL;
		uint32_t ccm_iv_len;

		if (kcapi_aead_ccm_nonce_to_iv(nonce, nonce_size, &ccm_iv,
					       &ccm_iv_len)) {
			gnutls_assert();
			ret = GNUTLS_E_DECRYPTION_FAILED;
			goto end;
		}
		if (kcapi_aead_stream_init_dec(ctx->handle, ccm_iv, NULL, 0) <
		    0) {
			free(ccm_iv);
			gnutls_assert();
			ret = GNUTLS_E_DECRYPTION_FAILED;
			goto end;
		}
		free(ccm_iv);
	} else {
		if (kcapi_aead_stream_init_dec(ctx->handle, nonce, NULL, 0) <
		    0) {
			gnutls_assert();
			ret = GNUTLS_E_DECRYPTION_FAILED;
			goto end;
		}
	}

	/*
	 * Set AAD: IOVECs do not support const, this buffer is guaranteed to be
	 * read-only
	 */
	iov[0].iov_base = (void *)auth;
	iov[0].iov_len = auth_size;

	/*
	 * Set CT: IOVECs do not support const, this buffer is guaranteed to be
	 * read-only
	 */
	iov[1].iov_base = (void *)encr;
	iov[1].iov_len = encr_size;

	if (kcapi_aead_stream_update_last(ctx->handle, iov, 2) < 0) {
		gnutls_assert();
		ret = GNUTLS_E_DECRYPTION_FAILED;
		goto end;
	}

	iov[0].iov_base = authtmp;
	iov[0].iov_len = auth_size;

	/* Set PT buffer to be filled by kernel */
	uint32_t outbuflen = kcapi_aead_outbuflen_dec(ctx->handle,
						      encr_size - tag_size,
						      auth_size, tag_size) -
			     auth_size;
	iov[1].iov_base = (void *)plain;
	iov[1].iov_len = (plain_size > outbuflen) ? outbuflen : plain_size;

	if (kcapi_aead_stream_op(ctx->handle, iov, 2) < 0) {
		gnutls_assert();
		ret = GNUTLS_E_DECRYPTION_FAILED;
		goto end;
	}

end:
	freea(authtmp);
	return ret;
}

static int afalg_aead_encrypt(void *_ctx, const void *nonce, size_t nonce_size,
			      const void *auth, size_t auth_size,
			      size_t tag_size, const void *plain,
			      size_t plain_size, void *encr, size_t encr_size)
{
	int ret = 0;
	struct kcapi_aead_ctx *ctx = _ctx;
	struct iovec iov[3];
	uint32_t iovlen = 2;
	uint8_t *authtmp = malloca(auth_size);
	if (authtmp == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	if (unlikely(encr_size - tag_size < plain_size)) {
		ret = GNUTLS_E_SHORT_MEMORY_BUFFER;
		gnutls_assert();
		goto end;
	}

	if (nonce_size > kcapi_aead_ivsize(ctx->handle)) {
		ret = GNUTLS_E_INVALID_REQUEST;
		gnutls_assert();
		goto end;
	}

	/* Init taglen once. */
	if (!ctx->taglen_set) {
		ctx->taglen_set = 1;

		if (kcapi_aead_settaglen(ctx->handle, tag_size) < 0) {
			gnutls_assert();
			ret = GNUTLS_E_ENCRYPTION_FAILED;
			goto end;
		}
	}

	kcapi_aead_setassoclen(ctx->handle, auth_size);

	/* CCM nonce to IV conversion */
	if (ctx->ccm) {
		uint8_t *ccm_iv = NULL;
		uint32_t ccm_iv_len;

		if (kcapi_aead_ccm_nonce_to_iv(nonce, nonce_size, &ccm_iv,
					       &ccm_iv_len)) {
			gnutls_assert();
			ret = GNUTLS_E_ENCRYPTION_FAILED;
			goto end;
		}
		if (kcapi_aead_stream_init_enc(ctx->handle, ccm_iv, NULL, 0) <
		    0) {
			free(ccm_iv);
			gnutls_assert();
			ret = GNUTLS_E_ENCRYPTION_FAILED;
			goto end;
		}
		free(ccm_iv);
	} else {
		if (kcapi_aead_stream_init_enc(ctx->handle, nonce, NULL, 0) <
		    0) {
			gnutls_assert();
			ret = GNUTLS_E_ENCRYPTION_FAILED;
			goto end;
		}
	}

	/*
	 * Set AAD: IOVECs do not support const, this buffer is guaranteed to be
	 * read-only
	 */
	iov[0].iov_base = (void *)auth;
	iov[0].iov_len = auth_size;

	/*
	 * Set PT: IOVECs do not support const, this buffer is guaranteed to be
	 * read-only
	 */
	iov[1].iov_base = (void *)plain;
	iov[1].iov_len = plain_size;

	/*
	 * Older kernels require tag as input. This buffer data is unused
	 * which implies the encr buffer can serve as tmp space.
	 */
	uint32_t inbuflen = kcapi_aead_inbuflen_enc(ctx->handle, plain_size,
						    auth_size, tag_size);
	if ((auth_size + plain_size) < inbuflen) {
		iov[2].iov_base = encr;
		iov[2].iov_len = tag_size;
		iovlen = 3;
	}

	if (kcapi_aead_stream_update_last(ctx->handle, iov, iovlen) < 0) {
		gnutls_assert();
		ret = GNUTLS_E_ENCRYPTION_FAILED;
		goto end;
	}

	iov[0].iov_base = authtmp;
	iov[0].iov_len = auth_size;

	/* Set CT buffer to be filled by kernel */
	uint32_t outbuflen = kcapi_aead_outbuflen_enc(ctx->handle, plain_size,
						      auth_size, tag_size) -
			     auth_size;

	iov[1].iov_base = encr;
	iov[1].iov_len = (encr_size > outbuflen) ? outbuflen : encr_size;

	if (kcapi_aead_stream_op(ctx->handle, iov, 2) < 0) {
		gnutls_assert();
		ret = GNUTLS_E_ENCRYPTION_FAILED;
		goto end;
	}

end:
	freea(authtmp);
	return ret;
}

static const gnutls_crypto_cipher_st afalg_aead_struct = {
	.init = afalg_aead_init,
	.setkey = afalg_aead_setkey,
	.aead_encrypt = afalg_aead_encrypt,
	.aead_decrypt = afalg_aead_decrypt,
	.deinit = afalg_aead_deinit,
};

static int afalg_aead_register(void)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < sizeof(gnutls_aead_map) / sizeof(gnutls_aead_map[0]);
	     i++) {
		struct kcapi_handle *handle;

		if (gnutls_aead_map[i] == 0)
			continue;

		/* Check whether cipher is available. */
		if (kcapi_aead_init(&handle, gnutls_aead_map[i], 0))
			continue;

		kcapi_aead_destroy(handle);

		_gnutls_debug_log("afalg: registering: %s\n",
				  gnutls_cipher_get_name(i));
		ret = gnutls_crypto_single_cipher_register(
			i, 90, &afalg_aead_struct, 0);
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}
	}

	return ret;
}

/********************** Keyed message digest algorithms **********************/

static const char *gnutls_mac_map[] = {
	[GNUTLS_MAC_SHA1] = "hmac(sha1)",
	[GNUTLS_MAC_SHA256] = "hmac(sha256)",
	[GNUTLS_MAC_SHA384] = "hmac(sha384)",
	[GNUTLS_MAC_SHA512] = "hmac(sha512)",
};

static int afalg_mac_init(gnutls_mac_algorithm_t algorithm, void **ctx)
{
	struct kcapi_handle *handle;

	if (kcapi_md_init(&handle, gnutls_mac_map[algorithm], 0) < 0) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	*ctx = handle;

	return 0;
}

static int afalg_mac_setkey(void *ctx, const void *key, size_t keysize)
{
	struct kcapi_handle *handle = ctx;

	if (kcapi_md_setkey(handle, key, keysize) < 0) {
		gnutls_assert();
		return GNUTLS_E_ENCRYPTION_FAILED;
	}

	return 0;
}

static int afalg_mac_hash(void *ctx, const void *_text, size_t textsize)
{
	struct kcapi_handle *handle = ctx;
	const uint8_t *text = _text;
	size_t offset;

	for (offset = 0; offset < textsize - textsize % INT_MAX;
	     offset += INT_MAX) {
		if (kcapi_md_update(handle, text + offset, INT_MAX) < 0) {
			return gnutls_assert_val(GNUTLS_E_ENCRYPTION_FAILED);
		}
	}

	if (offset < textsize) {
		if (kcapi_md_update(handle, text + offset, textsize - offset) <
		    0) {
			return gnutls_assert_val(GNUTLS_E_ENCRYPTION_FAILED);
		}
	}

	return 0;
}

static int afalg_mac_output(void *ctx, void *digest, size_t digestsize)
{
	struct kcapi_handle *handle = ctx;

	if (kcapi_md_final(handle, digest, digestsize) < 0) {
		gnutls_assert();
		return GNUTLS_E_ENCRYPTION_FAILED;
	}

	return 0;
}

static void afalg_mac_deinit(void *ctx)
{
	struct kcapi_handle *handle = ctx;

	kcapi_md_destroy(handle);
}

static int afalg_mac_fast(gnutls_mac_algorithm_t algorithm, const void *nonce,
			  size_t nonce_size, const void *key, size_t keysize,
			  const void *text, size_t textsize, void *digest)
{
	struct kcapi_handle *handle;
	int ret = GNUTLS_E_ENCRYPTION_FAILED;

	if (kcapi_md_init(&handle, gnutls_mac_map[algorithm], 0) < 0) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	if (kcapi_md_setkey(handle, key, keysize) < 0) {
		gnutls_assert();
		goto out;
	}

	if (textsize <= INT_MAX) {
		if (kcapi_md_digest(handle, text, textsize, digest,
				    kcapi_md_digestsize(handle)) < 0) {
			gnutls_assert();
			goto out;
		}
	} else {
		ret = afalg_mac_hash(handle, text, textsize);
		if (ret < 0) {
			goto out;
		}
		if (kcapi_md_final(handle, digest,
				   kcapi_md_digestsize(handle)) < 0) {
			gnutls_assert();
			return GNUTLS_E_ENCRYPTION_FAILED;
		}
	}

	ret = 0;

out:
	kcapi_md_destroy(handle);

	return ret;
}

static const gnutls_crypto_mac_st afalg_mac_struct = {
	.init = afalg_mac_init,
	.setkey = afalg_mac_setkey,
	.setnonce = NULL,
	.hash = afalg_mac_hash,
	.output = afalg_mac_output,
	.deinit = afalg_mac_deinit,
	.fast = afalg_mac_fast,
};

static int afalg_mac_register(void)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < sizeof(gnutls_mac_map) / sizeof(gnutls_mac_map[0]);
	     i++) {
		struct kcapi_handle *handle;

		if (gnutls_mac_map[i] == 0)
			continue;

		/* Check whether cipher is available. */
		if (kcapi_md_init(&handle, gnutls_mac_map[i], 0))
			continue;

		kcapi_md_destroy(handle);

		_gnutls_debug_log("afalg: registering: %s\n",
				  gnutls_mac_get_name(i));
		ret = gnutls_crypto_single_mac_register(i, 90,
							&afalg_mac_struct, 0);
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}
	}

	return ret;
}

/***************************** Digest algorithms *****************************/

static const char *gnutls_digest_map[] = {
	[GNUTLS_DIG_SHA1] = "sha1",
	[GNUTLS_DIG_SHA256] = "sha256",
	[GNUTLS_DIG_SHA384] = "sha384",
	[GNUTLS_DIG_SHA512] = "sha512",
};

static int afalg_digest_init(gnutls_digest_algorithm_t algorithm, void **ctx)
{
	struct kcapi_handle *handle;

	if (kcapi_md_init(&handle, gnutls_digest_map[algorithm], 0) < 0) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	*ctx = handle;

	return 0;
}

static int afalg_digest_fast(gnutls_digest_algorithm_t algorithm,
			     const void *text, size_t textsize, void *digest)
{
	struct kcapi_handle *handle;
	int ret = GNUTLS_E_ENCRYPTION_FAILED;

	if (kcapi_md_init(&handle, gnutls_digest_map[algorithm], 0) < 0) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	if (textsize <= INT_MAX) {
		if (kcapi_md_digest(handle, text, textsize, digest,
				    kcapi_md_digestsize(handle)) < 0) {
			gnutls_assert();
			goto out;
		}
	} else {
		ret = afalg_mac_hash(handle, text, textsize);
		if (ret < 0) {
			goto out;
		}

		if (kcapi_md_final(handle, digest,
				   kcapi_md_digestsize(handle)) < 0) {
			gnutls_assert();
			return GNUTLS_E_ENCRYPTION_FAILED;
		}
	}

	ret = 0;

out:
	kcapi_md_destroy(handle);

	return ret;
}

static const gnutls_crypto_digest_st afalg_digest_struct = {
	.init = afalg_digest_init,
	.hash = afalg_mac_hash,
	.output = afalg_mac_output,
	.deinit = afalg_mac_deinit,
	.fast = afalg_digest_fast
};

static int afalg_digest_register(void)
{
	unsigned int i;
	int ret = 0;

	for (i = 0;
	     i < sizeof(gnutls_digest_map) / sizeof(gnutls_digest_map[0]);
	     i++) {
		struct kcapi_handle *handle;

		if (gnutls_digest_map[i] == 0)
			continue;

		/* Check whether cipher is available. */
		if (kcapi_md_init(&handle, gnutls_digest_map[i], 0))
			continue;

		kcapi_md_destroy(handle);

		_gnutls_debug_log("afalg: registering: %s\n",
				  gnutls_digest_get_name(i));
		ret = gnutls_crypto_single_digest_register(
			i, 90, &afalg_digest_struct, 0);
		if (ret < 0) {
			gnutls_assert();
			return ret;
		}
	}

	return ret;
}

int _gnutls_afalg_init(void)
{
	int ret;

	ret = afalg_cipher_register();
	if (ret)
		return ret;

	ret = afalg_aead_register();
	if (ret)
		return ret;

	ret = afalg_mac_register();
	if (ret)
		return ret;

	return afalg_digest_register();
}

void _gnutls_afalg_deinit(void)
{
	return;
}

#else /* ENABLE_AFALG */

int _gnutls_afalg_init(void)
{
	return 0;
}

void _gnutls_afalg_deinit(void)
{
	return;
}

#endif /* ENABLE_AFALG */
