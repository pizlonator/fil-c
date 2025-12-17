/*
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * Author: Zoltan Fridrich
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS. If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnutls/crypto.h>

#include "utils.h"

static void test_xts_check(gnutls_cipher_algorithm_t alg)
{
	int ret;
	gnutls_cipher_hd_t ctx;
	gnutls_datum_t key, iv;

	iv.size = gnutls_cipher_get_iv_size(alg);
	iv.data = gnutls_malloc(iv.size);
	if (iv.data == NULL)
		fail("Error: %s\n", gnutls_strerror(GNUTLS_E_MEMORY_ERROR));
	gnutls_memset(iv.data, 0xf0, iv.size);

	key.size = gnutls_cipher_get_key_size(alg);
	key.data = gnutls_malloc(key.size);
	if (key.data == NULL) {
		gnutls_free(iv.data);
		fail("Error: %s\n", gnutls_strerror(GNUTLS_E_MEMORY_ERROR));
	}
	gnutls_memset(key.data, 0xf0, key.size);

	ret = gnutls_cipher_init(&ctx, alg, &key, &iv);
	if (ret == GNUTLS_E_SUCCESS) {
		gnutls_cipher_deinit(ctx);
		gnutls_free(iv.data);
		gnutls_free(key.data);
		fail("cipher initialization should fail for key1 == key2\n");
	}

	key.data[0] = 0xff;

	ret = gnutls_cipher_init(&ctx, alg, &key, &iv);
	gnutls_free(iv.data);
	gnutls_free(key.data);

	if (ret == GNUTLS_E_SUCCESS)
		gnutls_cipher_deinit(ctx);
	else
		fail("cipher initialization should succeed with key1 != key2"
		     "\n%s\n",
		     gnutls_strerror(ret));
}

void doit(void)
{
	if (!gnutls_fips140_mode_enabled())
		exit(77);

	test_xts_check(GNUTLS_CIPHER_AES_128_XTS);
	test_xts_check(GNUTLS_CIPHER_AES_256_XTS);
}
