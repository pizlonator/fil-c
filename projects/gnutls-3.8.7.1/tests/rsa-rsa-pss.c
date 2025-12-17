/*
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos
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

const gnutls_datum_t raw_data = { (void *)"hello there", 11 };

static gnutls_fips140_context_t fips_context;

static void inv_sign_check(unsigned sigalgo, gnutls_privkey_t privkey,
			   int exp_error)
{
	int ret;
	gnutls_datum_t signature;

	ret = gnutls_privkey_sign_data2(privkey, sigalgo, 0, &raw_data,
					&signature);
	if (ret != exp_error)
		fail("gnutls_privkey_sign_data succeeded with %s and %s: %s\n",
		     gnutls_pk_get_name(
			     gnutls_privkey_get_pk_algorithm(privkey, NULL)),
		     gnutls_sign_get_name(sigalgo), gnutls_strerror(ret));

	if (ret == 0)
		gnutls_free(signature.data);
}

static void inv_encryption_check(gnutls_pk_algorithm_t algorithm,
				 gnutls_privkey_t privkey, int exp_error)
{
	int ret;
	gnutls_datum_t ct;
	gnutls_pubkey_t pubkey;

	assert(gnutls_pubkey_init(&pubkey) >= 0);

	ret = gnutls_pubkey_import_privkey(pubkey, privkey, 0, 0);
	if (ret < 0)
		fail("gnutls_pubkey_import_privkey\n");

	ret = gnutls_pubkey_encrypt_data(pubkey, 0, &raw_data, &ct);
	if (ret != exp_error)
		fail("gnutls_pubkey_encrypt_data succeeded with %s: %s\n",
		     gnutls_pk_get_name(algorithm), gnutls_strerror(ret));

	gnutls_pubkey_deinit(pubkey);
}

static void sign_verify_data(unsigned sigalgo, gnutls_privkey_t privkey,
			     unsigned int sign_flags, unsigned int verify_flags,
			     int sign_exp_error, int verify_exp_error,
			     gnutls_fips140_operation_state_t sign_exp_state)
{
	int ret;
	gnutls_datum_t signature = { NULL, 0 };

	fips_push_context(fips_context);
	ret = gnutls_privkey_sign_data2(privkey, sigalgo, sign_flags, &raw_data,
					&signature);
	fips_pop_context(fips_context, sign_exp_state);
	if (ret != sign_exp_error)
		fail("gnutls_x509_privkey_sign_data returned unexpected error: %s\n",
		     gnutls_strerror(ret));

	if (ret < 0) {
		success("skipping verification as signing is expected to fail\n");
	} else {
		gnutls_pubkey_t pubkey;

		/* verify data */
		assert(gnutls_pubkey_init(&pubkey) >= 0);

		ret = gnutls_pubkey_import_privkey(pubkey, privkey, 0, 0);
		if (ret < 0)
			fail("gnutls_pubkey_import_privkey\n");

		ret = gnutls_pubkey_verify_data2(pubkey, sigalgo, verify_flags,
						 &raw_data, &signature);
		if (ret != verify_exp_error)
			fail("gnutls_pubkey_verify_data2 returned unexpected error: %s\n",
			     gnutls_strerror(ret));

		gnutls_pubkey_deinit(pubkey);
	}

	gnutls_free(signature.data);
}

static void prepare_keys(gnutls_privkey_t *pkey_rsa_pss,
			 gnutls_privkey_t *pkey_rsa,
			 gnutls_digest_algorithm_t dig, size_t salt_size)
{
	gnutls_privkey_t pkey;
	gnutls_x509_privkey_t tkey;
	int ret;
	gnutls_x509_spki_t spki;
	gnutls_datum_t tmp;

	assert(gnutls_x509_spki_init(&spki) >= 0);

	assert(gnutls_privkey_init(&pkey) >= 0);

	gnutls_x509_spki_set_rsa_pss_params(spki, dig, salt_size);

	ret = gnutls_privkey_generate(pkey, GNUTLS_PK_RSA, 2048, 0);
	if (ret < 0) {
		fail("gnutls_privkey_generate: %s\n", gnutls_strerror(ret));
	}

	assert(gnutls_privkey_set_spki(pkey, spki, 0) >= 0);
	assert(gnutls_privkey_export_x509(pkey, &tkey) >= 0);
	gnutls_x509_spki_deinit(spki);

	gnutls_x509_privkey_export2_pkcs8(tkey, GNUTLS_X509_FMT_PEM, NULL, 0,
					  &tmp);

	/* import RSA-PSS version of key */
	assert(gnutls_privkey_init(pkey_rsa_pss) >= 0);
	assert(gnutls_privkey_import_x509_raw(
		       *pkey_rsa_pss, &tmp, GNUTLS_X509_FMT_PEM, NULL, 0) >= 0);

	gnutls_free(tmp.data);

	/* import RSA version of key */
	gnutls_x509_privkey_export2(tkey, GNUTLS_X509_FMT_PEM, &tmp);
	assert(gnutls_privkey_init(pkey_rsa) >= 0);
	assert(gnutls_privkey_import_x509_raw(
		       *pkey_rsa, &tmp, GNUTLS_X509_FMT_PEM, NULL, 0) >= 0);

	gnutls_x509_privkey_deinit(tkey);
	gnutls_free(tmp.data);
	gnutls_privkey_deinit(pkey);
}

void doit(void)
{
	gnutls_privkey_t pkey_rsa_pss;
	gnutls_privkey_t pkey_rsa;
	gnutls_x509_spki_t spki;
	int ret;

	ret = global_init();
	if (ret < 0)
		fail("global_init: %d\n", ret);

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	assert(gnutls_fips140_context_init(&fips_context) >= 0);

	prepare_keys(&pkey_rsa_pss, &pkey_rsa, GNUTLS_DIG_SHA256, 32);

	sign_verify_data(GNUTLS_SIGN_RSA_PSS_SHA256, pkey_rsa_pss, 0, 0, 0, 0,
			 GNUTLS_FIPS140_OP_APPROVED);
	sign_verify_data(GNUTLS_SIGN_RSA_PSS_RSAE_SHA256, pkey_rsa, 0, 0, 0, 0,
			 GNUTLS_FIPS140_OP_APPROVED);
	sign_verify_data(GNUTLS_SIGN_RSA_PSS_SHA256, pkey_rsa, 0, 0, 0, 0,
			 GNUTLS_FIPS140_OP_APPROVED);

	if (debug)
		success("success signing with RSA-PSS-SHA256\n");

	/* check whether the RSA-PSS key restrictions are being followed */
	inv_encryption_check(GNUTLS_PK_RSA_PSS, pkey_rsa_pss,
			     GNUTLS_E_INVALID_REQUEST);
	inv_sign_check(GNUTLS_SIGN_RSA_SHA512, pkey_rsa_pss,
		       GNUTLS_E_CONSTRAINT_ERROR);
	inv_sign_check(GNUTLS_SIGN_RSA_SHA256, pkey_rsa_pss,
		       GNUTLS_E_CONSTRAINT_ERROR);
	inv_sign_check(GNUTLS_SIGN_RSA_PSS_SHA384, pkey_rsa_pss,
		       GNUTLS_E_CONSTRAINT_ERROR);
	inv_sign_check(GNUTLS_SIGN_RSA_PSS_SHA512, pkey_rsa_pss,
		       GNUTLS_E_CONSTRAINT_ERROR);
	inv_sign_check(GNUTLS_SIGN_RSA_PSS_RSAE_SHA384, pkey_rsa_pss,
		       GNUTLS_E_CONSTRAINT_ERROR);
	inv_sign_check(GNUTLS_SIGN_RSA_PSS_RSAE_SHA512, pkey_rsa_pss,
		       GNUTLS_E_CONSTRAINT_ERROR);

	/* check whether the RSA key is not being restricted */
	inv_sign_check(GNUTLS_SIGN_RSA_SHA512, pkey_rsa, 0);
	inv_sign_check(GNUTLS_SIGN_RSA_SHA256, pkey_rsa, 0);
	inv_sign_check(GNUTLS_SIGN_RSA_PSS_RSAE_SHA384, pkey_rsa, 0);
	inv_sign_check(GNUTLS_SIGN_RSA_PSS_RSAE_SHA512, pkey_rsa, 0);
	/* an RSA key can also generate "pure" for TLS RSA-PSS signatures
	 * as they are essentially the same thing, and we cannot always
	 * know whether a key is RSA-PSS only, or not (e.g., in PKCS#11
	 * keys). */
	inv_sign_check(GNUTLS_SIGN_RSA_PSS_SHA384, pkey_rsa, 0);
	inv_sign_check(GNUTLS_SIGN_RSA_PSS_SHA512, pkey_rsa, 0);

	gnutls_privkey_deinit(pkey_rsa_pss);
	gnutls_privkey_deinit(pkey_rsa);

	/* Restrict key to use salt length larger than hash output
	 * length (not approved in FIPS).
	 */
	prepare_keys(&pkey_rsa_pss, &pkey_rsa, GNUTLS_DIG_SHA256, 33);

	sign_verify_data(GNUTLS_SIGN_RSA_PSS_SHA256, pkey_rsa_pss, 0, 0, 0, 0,
			 GNUTLS_FIPS140_OP_NOT_APPROVED);

	gnutls_privkey_deinit(pkey_rsa_pss);
	gnutls_privkey_deinit(pkey_rsa);

	/* Use the mismatched salt length with the digest length */
	prepare_keys(&pkey_rsa_pss, &pkey_rsa, GNUTLS_DIG_SHA256, 48);

	sign_verify_data(GNUTLS_SIGN_RSA_PSS_RSAE_SHA256, pkey_rsa_pss, 0, 0, 0,
			 0, GNUTLS_FIPS140_OP_NOT_APPROVED);
	sign_verify_data(GNUTLS_SIGN_RSA_PSS_SHA256, pkey_rsa_pss,
			 GNUTLS_PRIVKEY_FLAG_RSA_PSS_FIXED_SALT_LENGTH, 0,
			 GNUTLS_E_CONSTRAINT_ERROR, 0,
			 /* The error is caught before calling the actual
			  * signing operation.
			  */
			 GNUTLS_FIPS140_OP_INITIAL);
	sign_verify_data(GNUTLS_SIGN_RSA_PSS_SHA256, pkey_rsa_pss, 0,
			 GNUTLS_VERIFY_RSA_PSS_FIXED_SALT_LENGTH, 0,
			 GNUTLS_E_PK_SIG_VERIFY_FAILED,
			 GNUTLS_FIPS140_OP_NOT_APPROVED);

	assert(gnutls_x509_spki_init(&spki) >= 0);
	gnutls_x509_spki_set_rsa_pss_params(spki, GNUTLS_DIG_SHA256, 48);
	assert(gnutls_privkey_set_spki(pkey_rsa, spki, 0) >= 0);

	sign_verify_data(GNUTLS_SIGN_RSA_PSS_RSAE_SHA256, pkey_rsa, 0, 0, 0, 0,
			 GNUTLS_FIPS140_OP_NOT_APPROVED);
	sign_verify_data(GNUTLS_SIGN_RSA_PSS_RSAE_SHA256, pkey_rsa,
			 GNUTLS_PRIVKEY_FLAG_RSA_PSS_FIXED_SALT_LENGTH, 0,
			 GNUTLS_E_CONSTRAINT_ERROR, 0,
			 /* The error is caught before calling the actual
			  * signing operation.
			  */
			 GNUTLS_FIPS140_OP_INITIAL);
	sign_verify_data(GNUTLS_SIGN_RSA_PSS_RSAE_SHA256, pkey_rsa, 0,
			 GNUTLS_VERIFY_RSA_PSS_FIXED_SALT_LENGTH, 0,
			 GNUTLS_E_PK_SIG_VERIFY_FAILED,
			 GNUTLS_FIPS140_OP_NOT_APPROVED);

	gnutls_privkey_deinit(pkey_rsa_pss);
	gnutls_privkey_deinit(pkey_rsa);
	gnutls_x509_spki_deinit(spki);

	gnutls_fips140_context_deinit(fips_context);

	gnutls_global_deinit();
}
