/*
 * Copyright (C) 2008-2012 Free Software Foundation, Inc.
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * Author: David Marín Carreño
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

#define MAX_TRIES 2

/* This tests the key generation, as well as the sign/verification
 * functionality of the supported public key algorithms.
 */

static int sec_param[MAX_TRIES] =
#ifdef ENABLE_FIPS140
	{ GNUTLS_SEC_PARAM_MEDIUM, GNUTLS_SEC_PARAM_HIGH };
#else
	{ GNUTLS_SEC_PARAM_LOW, GNUTLS_SEC_PARAM_MEDIUM };
#endif

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s |<%d>| %s", "privkey-keygen", level, str);
}

const gnutls_datum_t raw_data = { (void *)"hello there", 11 };

static void sign_verify_data(gnutls_pk_algorithm_t algorithm,
			     gnutls_x509_privkey_t pkey)
{
	int ret;
	gnutls_privkey_t privkey;
	gnutls_pubkey_t pubkey;
	gnutls_datum_t signature;
	gnutls_digest_algorithm_t digest;
	unsigned vflags = 0;

	assert(gnutls_privkey_init(&privkey) >= 0);

	ret = gnutls_privkey_import_x509(privkey, pkey, 0);
	if (ret < 0)
		fail("gnutls_privkey_import_x509\n");

	assert(gnutls_pubkey_init(&pubkey) >= 0);

	ret = gnutls_pubkey_import_privkey(pubkey, privkey, 0, 0);
	if (ret < 0)
		fail("gnutls_pubkey_import_privkey\n");

	ret = gnutls_pubkey_get_preferred_hash_algorithm(pubkey, &digest, NULL);
	if (ret < 0)
		fail("gnutls_pubkey_get_preferred_hash_algorithm\n");

	if (digest == GNUTLS_DIG_GOSTR_94)
		vflags |= GNUTLS_VERIFY_ALLOW_BROKEN;

	/* sign arbitrary data */
	ret = gnutls_privkey_sign_data(privkey, digest, 0, &raw_data,
				       &signature);
	if (ret < 0)
		fail("gnutls_privkey_sign_data\n");

	/* verify data */
	ret = gnutls_pubkey_verify_data2(
		pubkey,
		gnutls_pk_to_sign(gnutls_pubkey_get_pk_algorithm(pubkey, NULL),
				  digest),
		vflags, &raw_data, &signature);
	if (ret < 0)
		fail("gnutls_pubkey_verify_data2\n");

	gnutls_pubkey_deinit(pubkey);
	gnutls_privkey_deinit(privkey);
	gnutls_free(signature.data);
}

static unsigned int is_approved_pk_algo(gnutls_pk_algorithm_t algo)
{
	switch (algo) {
	case GNUTLS_PK_RSA:
	case GNUTLS_PK_RSA_PSS:
	case GNUTLS_PK_RSA_OAEP:
	case GNUTLS_PK_EC:
		return 1;
	default:
		return 0;
	}
}

void doit(void)
{
	gnutls_x509_privkey_t pkey, dst;
	int ret, algorithm, i;
	gnutls_fips140_context_t fips_context;

	ret = global_init();
	if (ret < 0)
		fail("global_init: %d\n", ret);

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	ret = gnutls_fips140_context_init(&fips_context);
	if (ret < 0) {
		fail("Cannot initialize FIPS context\n");
	}

	for (i = 0; i < MAX_TRIES; i++) {
		for (algorithm = GNUTLS_PK_RSA; algorithm <= GNUTLS_PK_MAX;
		     algorithm++) {
			if (algorithm == GNUTLS_PK_DH ||
#ifndef ENABLE_DSA
			    algorithm == GNUTLS_PK_DSA ||
#endif
			    algorithm == GNUTLS_PK_ECDH_X25519 ||
			    algorithm == GNUTLS_PK_ECDH_X448)
				continue;

			if (algorithm == GNUTLS_PK_GOST_01 ||
			    algorithm == GNUTLS_PK_GOST_12_256 ||
			    algorithm == GNUTLS_PK_GOST_12_512) {
				/* Skip GOST algorithms:
				 * - If they are disabled by ./configure option
				 * - Or in FIPS140 mode
				 */
#ifdef ENABLE_GOST
				if (gnutls_fips140_mode_enabled())
					continue;
#else
				continue;
#endif
			}

			ret = gnutls_x509_privkey_init(&pkey);
			if (ret < 0) {
				fail("gnutls_x509_privkey_init: %d\n", ret);
			}

			ret = gnutls_x509_privkey_init(&dst);
			if (ret < 0) {
				fail("gnutls_x509_privkey_init: %d\n", ret);
			}

			FIPS_PUSH_CONTEXT();
			ret = gnutls_x509_privkey_generate(
				pkey, algorithm,
				gnutls_sec_param_to_pk_bits(algorithm,
							    sec_param[i]),
				0);
			if (ret < 0) {
				fail("gnutls_x509_privkey_generate (%s-%d): %s (%d)\n",
				     gnutls_pk_algorithm_get_name(algorithm),
				     gnutls_sec_param_to_pk_bits(algorithm,
								 sec_param[i]),
				     gnutls_strerror(ret), ret);
			} else if (debug) {
				success("Key[%s] generation ok: %d\n",
					gnutls_pk_algorithm_get_name(algorithm),
					ret);
			}
			if (is_approved_pk_algo(algorithm)) {
				FIPS_POP_CONTEXT(APPROVED);
			} else {
				FIPS_POP_CONTEXT(NOT_APPROVED);
			}

			ret = gnutls_x509_privkey_verify_params(pkey);
			if (ret < 0) {
				fail("gnutls_x509_privkey_generate (%s): %s (%d)\n",
				     gnutls_pk_algorithm_get_name(algorithm),
				     gnutls_strerror(ret), ret);
			}

			/* include test of cpy */
			ret = gnutls_x509_privkey_cpy(dst, pkey);
			if (ret < 0) {
				fail("gnutls_x509_privkey_cpy (%s): %s (%d)\n",
				     gnutls_pk_algorithm_get_name(algorithm),
				     gnutls_strerror(ret), ret);
			}

			ret = gnutls_x509_privkey_verify_params(pkey);
			if (ret < 0) {
				fail("gnutls_x509_privkey_generate after cpy (%s): %s (%d)\n",
				     gnutls_pk_algorithm_get_name(algorithm),
				     gnutls_strerror(ret), ret);
			}

			/* RSA-OAEP doesn't support signing */
			if (algorithm == GNUTLS_PK_RSA_OAEP) {
				goto end;
			}

			FIPS_PUSH_CONTEXT();
			sign_verify_data(algorithm, pkey);
			if (is_approved_pk_algo(algorithm)) {
				FIPS_POP_CONTEXT(APPROVED);
			} else {
				FIPS_POP_CONTEXT(NOT_APPROVED);
			}

			FIPS_PUSH_CONTEXT();
			sign_verify_data(algorithm, dst);
			if (is_approved_pk_algo(algorithm)) {
				FIPS_POP_CONTEXT(APPROVED);
			} else {
				FIPS_POP_CONTEXT(NOT_APPROVED);
			}

		end:
			gnutls_x509_privkey_deinit(pkey);
			gnutls_x509_privkey_deinit(dst);
			success("Generated key with %s-%d\n",
				gnutls_pk_algorithm_get_name(algorithm),
				gnutls_sec_param_to_pk_bits(algorithm,
							    sec_param[i]));
		}
	}

	gnutls_fips140_context_deinit(fips_context);
	gnutls_global_deinit();
}
