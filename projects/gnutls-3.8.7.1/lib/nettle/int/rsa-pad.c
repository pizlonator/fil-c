/*
 * Copyright (C) 2021 Red Hat, Inc.
 *
 * Author: Daiki Ueno
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "gnutls_int.h"

#include <nettle/pkcs1.h>
#include <nettle/pss.h>
#include <nettle/sha2.h>

/* These are helper functions to perform RSA padding before signing, only used
 * for the crypto backends that do not support RSA-PKCS1/PSS natively for the
 * use with TLS (such as TPM2); not recommended for general usage.
 */

int _gnutls_rsa_pkcs1_sign_pad(size_t key_bits, const gnutls_datum_t *data,
			       unsigned char *buffer, size_t buffer_size)
{
	size_t key_size = (key_bits + 7) / 8;
	size_t size;
	mpz_t m;
	int ret = 0;

	mpz_init(m);
	if (!pkcs1_rsa_digest_encode(m, key_size, data->size, data->data)) {
		ret = gnutls_assert_val(GNUTLS_E_PK_SIGN_FAILED);
		goto out;
	}

	size = nettle_mpz_sizeinbase_256_u(m);
	if (size > buffer_size) {
		ret = gnutls_assert_val(GNUTLS_E_SHORT_MEMORY_BUFFER);
		goto out;
	}
	nettle_mpz_get_str_256(buffer_size, buffer, m);

out:
	mpz_clear(m);
	return ret;
}

int _gnutls_rsa_pss_sign_pad(gnutls_x509_spki_st *params, size_t key_bits,
			     const gnutls_datum_t *data, unsigned char *buffer,
			     size_t buffer_size)
{
	mpz_t m;
	int ret = 0;
	const struct nettle_hash *hash;
	uint8_t salt[SHA512_DIGEST_SIZE];
	size_t size;

	mpz_init(m);

	switch (params->rsa_pss_dig) {
	case GNUTLS_DIG_SHA256:
		hash = &nettle_sha256;
		break;
	case GNUTLS_DIG_SHA384:
		hash = &nettle_sha384;
		break;
	case GNUTLS_DIG_SHA512:
		hash = &nettle_sha512;
		break;
	default:
		ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
		goto out;
	}

	if (data->size != hash->digest_size) {
		ret = gnutls_assert_val(GNUTLS_E_INVALID_REQUEST);
		goto out;
	}

	ret = gnutls_rnd(GNUTLS_RND_NONCE, salt, params->salt_size);
	if (ret < 0) {
		goto out;
	}

	/* The emBits for EMSA-PSS encoding is actually one *fewer*
	 * bit than the RSA modulus. */
	if (!pss_encode_mgf1(m, key_bits - 1, hash, params->salt_size, salt,
			     data->data)) {
		ret = gnutls_assert_val(GNUTLS_E_PK_SIGN_FAILED);
		goto out;
	}

	size = nettle_mpz_sizeinbase_256_u(m);
	if (size > buffer_size) {
		ret = gnutls_assert_val(GNUTLS_E_SHORT_MEMORY_BUFFER);
		goto out;
	}
	nettle_mpz_get_str_256(buffer_size, buffer, m);

out:
	mpz_clear(m);
	return ret;
}
