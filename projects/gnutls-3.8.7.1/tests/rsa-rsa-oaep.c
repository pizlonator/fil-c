/*
 * Copyright (C) 2024 Red Hat, Inc.
 *
 * Author: Daiki Ueno
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>
#include <assert.h>

#include "utils.h"

/* This tests the key conversion from basic RSA to RSA-PSS.
 */

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "<%d>| %s", level, str);
}

const gnutls_datum_t plaintext = { (void *)"hello there", 11 };
const gnutls_datum_t label_data = { (void *)"label", 5 };

static gnutls_fips140_context_t fips_context;

static void encrypt_decrypt_data(gnutls_privkey_t privkey,
				 gnutls_fips140_operation_state_t exp_state)
{
	int ret;
	gnutls_pubkey_t pubkey;
	gnutls_datum_t ciphertext = { NULL, 0 };
	gnutls_datum_t decrypted = { NULL, 0 };
	gnutls_datum_t uninitialized;

	assert(gnutls_pubkey_init(&pubkey) >= 0);
	ret = gnutls_pubkey_import_privkey(pubkey, privkey, 0, 0);
	if (ret < 0)
		fail("gnutls_pubkey_import_privkey\n");

	fips_push_context(fips_context);
	ret = gnutls_pubkey_encrypt_data(pubkey, 0, &plaintext, &ciphertext);
	fips_pop_context(fips_context, exp_state);

	if (exp_state == GNUTLS_FIPS140_OP_ERROR)
		goto out;

	fips_push_context(fips_context);
	ret = gnutls_privkey_decrypt_data(privkey, 0, &ciphertext, &decrypted);
	fips_pop_context(fips_context, exp_state);

	/* gnutls_privkey_decrypt_data shouldn't touch plaintext upon
	 * failure */
	assert(ciphertext.size >= 4);
	memcpy(ciphertext.data, "\xde\xad\xbe\xef", 4);
	ret = gnutls_privkey_decrypt_data(privkey, 0, &ciphertext,
					  &uninitialized);
	if (ret >= 0)
		fail("gnutls_privkey_decrypt_data unexpectedly succeeded\n");

out:
	gnutls_pubkey_deinit(pubkey);
	gnutls_free(ciphertext.data);
	gnutls_free(decrypted.data);
}

static void prepare_key(gnutls_privkey_t *priv, gnutls_digest_algorithm_t dig,
			const gnutls_datum_t *label)
{
	gnutls_x509_privkey_t tkey;
	int ret;
	gnutls_x509_spki_t spki;
	gnutls_datum_t tmp;

	assert(gnutls_privkey_init(priv) >= 0);

	ret = gnutls_privkey_generate(*priv, GNUTLS_PK_RSA, 2048, 0);
	if (ret < 0) {
		fail("gnutls_privkey_generate: %s\n", gnutls_strerror(ret));
	}

	assert(gnutls_x509_spki_init(&spki) >= 0);
	assert(gnutls_x509_spki_set_rsa_oaep_params(spki, dig, label) >= 0);

	assert(gnutls_privkey_set_spki(*priv, spki, 0) >= 0);
	gnutls_x509_spki_deinit(spki);

	assert(gnutls_privkey_export_x509(*priv, &tkey) >= 0);
	gnutls_privkey_deinit(*priv);

	assert(gnutls_x509_privkey_export2_pkcs8(tkey, GNUTLS_X509_FMT_PEM,
						 NULL, 0, &tmp) >= 0);
	gnutls_x509_privkey_deinit(tkey);

	assert(gnutls_privkey_init(priv) >= 0);
	assert(gnutls_privkey_import_x509_raw(*priv, &tmp, GNUTLS_X509_FMT_PEM,
					      NULL, 0) >= 0);
	gnutls_free(tmp.data);
}

void doit(void)
{
	gnutls_privkey_t priv;
	int ret;

	ret = global_init();
	if (ret < 0)
		fail("global_init: %d\n", ret);

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	assert(gnutls_fips140_context_init(&fips_context) >= 0);

	success("sha256, without label\n");
	prepare_key(&priv, GNUTLS_DIG_SHA256, NULL);
	encrypt_decrypt_data(priv, GNUTLS_FIPS140_OP_APPROVED);
	gnutls_privkey_deinit(priv);

	success("sha256, with label\n");
	prepare_key(&priv, GNUTLS_DIG_SHA256, &label_data);
	encrypt_decrypt_data(priv, GNUTLS_FIPS140_OP_APPROVED);
	gnutls_privkey_deinit(priv);

	success("sha384, without label\n");
	prepare_key(&priv, GNUTLS_DIG_SHA384, NULL);
	encrypt_decrypt_data(priv, GNUTLS_FIPS140_OP_APPROVED);
	gnutls_privkey_deinit(priv);

	success("sha384, with label\n");
	prepare_key(&priv, GNUTLS_DIG_SHA384, &label_data);
	encrypt_decrypt_data(priv, GNUTLS_FIPS140_OP_APPROVED);
	gnutls_privkey_deinit(priv);

	success("sha512, without label\n");
	prepare_key(&priv, GNUTLS_DIG_SHA512, NULL);
	encrypt_decrypt_data(priv, GNUTLS_FIPS140_OP_APPROVED);
	gnutls_privkey_deinit(priv);

	success("sha512, with label\n");
	prepare_key(&priv, GNUTLS_DIG_SHA512, &label_data);
	encrypt_decrypt_data(priv, GNUTLS_FIPS140_OP_APPROVED);
	gnutls_privkey_deinit(priv);

	/* SHA-1 is not supported with RSA-OAEP */
	success("sha1, without label\n");
	prepare_key(&priv, GNUTLS_DIG_SHA1, NULL);
	encrypt_decrypt_data(priv, GNUTLS_FIPS140_OP_ERROR);
	gnutls_privkey_deinit(priv);

	success("sha1, with label\n");
	prepare_key(&priv, GNUTLS_DIG_SHA1, &label_data);
	encrypt_decrypt_data(priv, GNUTLS_FIPS140_OP_ERROR);
	gnutls_privkey_deinit(priv);

	gnutls_fips140_context_deinit(fips_context);

	gnutls_global_deinit();
}
