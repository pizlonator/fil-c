/*
 * Copyright (C) 2019-2023 Red Hat, Inc.
 *
 * Author: Simo Sorce, Daiki Ueno
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
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>.
 */

/* This program tests functionality of DH exchanges, with public API */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"
#include "dh-compute.h"

static void params(gnutls_dh_params_t *dh_params, const gnutls_datum_t *p,
		   const gnutls_datum_t *q, const gnutls_datum_t *g,
		   int expect_error)
{
	int ret;

	ret = gnutls_dh_params_init(dh_params);
	if (ret != 0)
		fail("error\n");

	ret = gnutls_dh_params_import_raw3(*dh_params, p, q, g);
	if (ret != expect_error)
		fail("error %d (expected %d)\n", ret, expect_error);
}

static bool dh_params_equal(const gnutls_dh_params_t a,
			    const gnutls_dh_params_t b)
{
	gnutls_datum_t prime1, generator1;
	unsigned int bits1;
	gnutls_datum_t prime2, generator2;
	unsigned int bits2;
	int ret;
	bool ok;

	ret = gnutls_dh_params_export_raw(a, &prime1, &generator1, &bits1);
	assert(ret >= 0);
	ret = gnutls_dh_params_export_raw(b, &prime2, &generator2, &bits2);
	assert(ret >= 0);

	ok = prime1.size == prime2.size &&
	     !memcmp(prime1.data, prime2.data, prime1.size) &&
	     generator1.size == generator2.size &&
	     !memcmp(generator1.data, generator2.data, generator1.size) &&
	     bits1 == bits2;

	gnutls_free(prime1.data);
	gnutls_free(prime2.data);
	gnutls_free(generator1.data);
	gnutls_free(generator2.data);

	return ok;
}

static void genkey(const gnutls_dh_params_t dh_params, gnutls_datum_t *priv_key,
		   gnutls_datum_t *pub_key, int expect_error)
{
	int ret;
	gnutls_privkey_t privkey;
	gnutls_pubkey_t pubkey;
	gnutls_keygen_data_st data;
	gnutls_dh_params_t dh_params_exported;
	gnutls_datum_t tmp;

	ret = gnutls_privkey_init(&privkey);
	assert(ret >= 0);

	ret = gnutls_pubkey_init(&pubkey);
	assert(ret >= 0);

	data.type = GNUTLS_KEYGEN_DH;
	data.data = (unsigned char *)dh_params;
	ret = gnutls_privkey_generate2(privkey, GNUTLS_PK_DH, 0, 0, &data, 1);
	if (ret != expect_error)
		fail("error %d (expected %d)\n", ret, expect_error);

	ret = gnutls_pubkey_import_privkey(pubkey, privkey, 0, 0);
	assert(ret >= 0);

	/* Retrieve only private key */
	ret = gnutls_privkey_export_dh_raw(privkey, NULL, NULL, &tmp, 0);
	if (ret != 0) {
		fail("unable to export private key: %s\n",
		     gnutls_strerror(ret));
	}
	gnutls_free(tmp.data);

	/* Retrieve DH params and private key */
	ret = gnutls_dh_params_init(&dh_params_exported);
	assert(ret >= 0);

	ret = gnutls_privkey_export_dh_raw(privkey, dh_params_exported, NULL,
					   &tmp, 0);
	if (ret != 0) {
		fail("unable to export private key: %s\n",
		     gnutls_strerror(ret));
	}
	*priv_key = tmp;

	if (!dh_params_equal(dh_params_exported, dh_params)) {
		fail("error\n");
	}

	gnutls_dh_params_deinit(dh_params_exported);

	/* Retrieve only public key */
	ret = gnutls_pubkey_export_dh_raw(pubkey, NULL, &tmp, 0);
	if (ret != 0) {
		fail("unable to export public key: %s\n", gnutls_strerror(ret));
	}
	gnutls_free(tmp.data);

	/* Retrieve DH params and public key */
	ret = gnutls_dh_params_init(&dh_params_exported);
	assert(ret >= 0);

	ret = gnutls_pubkey_export_dh_raw(pubkey, dh_params_exported, &tmp, 0);
	if (ret != 0) {
		fail("unable to export public key: %s\n", gnutls_strerror(ret));
	}
	*pub_key = tmp;

	if (!dh_params_equal(dh_params_exported, dh_params)) {
		fail("error\n");
	}

	gnutls_dh_params_deinit(dh_params_exported);
	gnutls_privkey_deinit(privkey);
	gnutls_pubkey_deinit(pubkey);
}

static void compute_key(const char *name, const gnutls_dh_params_t dh_params,
			const gnutls_datum_t *priv_key,
			const gnutls_datum_t *pub_key,
			const gnutls_datum_t *peer_key,
			int expect_error_on_import, int expect_error_on_derive,
			const gnutls_datum_t *result)
{
	gnutls_datum_t Z = { 0 };
	bool ok;
	int ret;
	gnutls_privkey_t privkey = NULL;
	gnutls_pubkey_t pubkey = NULL;

	ret = gnutls_privkey_init(&privkey);
	assert(ret >= 0);

	ret = gnutls_privkey_import_dh_raw(privkey, dh_params, NULL, priv_key);
	assert(ret >= 0);

	ret = gnutls_pubkey_init(&pubkey);
	assert(ret >= 0);

	ret = gnutls_pubkey_import_dh_raw(pubkey, dh_params, peer_key);
	if (ret != expect_error_on_import)
		fail("%s: error %d (expected %d)\n", name, ret,
		     expect_error_on_import);

	if (expect_error_on_import != 0)
		goto cleanup;

	ret = gnutls_privkey_derive_secret(privkey, pubkey, NULL, &Z, 0);
	if (ret != expect_error_on_derive)
		fail("%s: error %d (expected %d)\n", name, ret,
		     expect_error_on_derive);

	if (expect_error_on_derive != 0)
		goto cleanup;

	if (result) {
		ok = Z.size == result->size &&
		     memcmp(Z.data, result->data, Z.size) == 0;
		if (!ok) {
			success("priv_key\n");
			hexprint(priv_key->data, priv_key->size);
			success("pub_key\n");
			hexprint(pub_key->data, pub_key->size);
			success("Z\n");
			hexprint(Z.data, Z.size);
			fail("%s: failed to match result\n", name);
		}
	}

cleanup:
	gnutls_free(Z.data);
	gnutls_privkey_deinit(privkey);
	gnutls_pubkey_deinit(pubkey);
}

struct dh_test_data_expected {
	int ret;
	int fips_ret;
	gnutls_fips140_operation_state_t state;
};

struct dh_test_data {
	const char *name;
	const gnutls_datum_t prime;
	const gnutls_datum_t q;
	const gnutls_datum_t generator;
	const gnutls_datum_t peer_key;
	const gnutls_datum_t priv_key;
	const gnutls_datum_t pub_key;
	const gnutls_datum_t result;
	struct dh_test_data_expected params_expected;
	struct dh_test_data_expected genkey_expected;
	struct dh_test_data_expected import_expected;
	struct dh_test_data_expected derive_expected;
};

static inline int get_expected(struct dh_test_data_expected *expected)
{
	return gnutls_fips140_mode_enabled() ? expected->fips_ret :
					       expected->ret;
}

void doit(void)
{
	struct dh_test_data test_data[] = {
		{
			"[y == 0]",
			gnutls_ffdhe_2048_group_prime,
			gnutls_ffdhe_2048_group_q,
			gnutls_ffdhe_2048_group_generator,
			{ (void *)"\x00", 1 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ 0, 0, GNUTLS_FIPS140_OP_INITIAL },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ GNUTLS_E_MPI_SCAN_FAILED, GNUTLS_E_MPI_SCAN_FAILED,
			  GNUTLS_FIPS140_OP_INITIAL },
		},
		{
			"[y < 2]",
			gnutls_ffdhe_2048_group_prime,
			gnutls_ffdhe_2048_group_q,
			gnutls_ffdhe_2048_group_generator,
			{ (void *)"\x01", 1 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ 0, 0, GNUTLS_FIPS140_OP_INITIAL },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER,
			  GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER,
			  GNUTLS_FIPS140_OP_ERROR },
		},
		{
			"[y > p - 2]",
			gnutls_ffdhe_2048_group_prime,
			gnutls_ffdhe_2048_group_q,
			gnutls_ffdhe_2048_group_generator,
			gnutls_ffdhe_2048_group_prime,
			{ NULL, 0 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ 0, 0, GNUTLS_FIPS140_OP_INITIAL },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER,
			  GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER,
			  GNUTLS_FIPS140_OP_ERROR },
		},
		{
			"[y ^ q mod p == 1]",
			gnutls_ffdhe_2048_group_prime,
			gnutls_ffdhe_2048_group_q,
			gnutls_ffdhe_2048_group_generator,
			gnutls_ffdhe_2048_group_q,
			{ NULL, 0 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ 0, 0, GNUTLS_FIPS140_OP_INITIAL },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER,
			  GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER,
			  GNUTLS_FIPS140_OP_ERROR },
		},
		{
			"Legal Input",
			gnutls_ffdhe_2048_group_prime,
			gnutls_ffdhe_2048_group_q,
			gnutls_ffdhe_2048_group_generator,
			{ (void *)"\x02", 1 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ 0, 0, GNUTLS_FIPS140_OP_INITIAL },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
		},
		{
			"Legal Input without Q",
			gnutls_ffdhe_2048_group_prime,
			{ NULL, 0 },
			gnutls_ffdhe_2048_group_generator,
			{ (void *)"\x02", 1 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ NULL, 0 },
			{ 0, GNUTLS_E_DH_PRIME_UNACCEPTABLE,
			  GNUTLS_FIPS140_OP_INITIAL },
		},
		{
			"Legal Input (KAT)",
			gnutls_ffdhe_2048_group_prime,
			gnutls_ffdhe_2048_group_q,
			gnutls_ffdhe_2048_group_generator,
			{ (void *)"\x02", 1 },
			{ (void *)ffdhe_2048_priv_key,
			  sizeof(ffdhe_2048_priv_key) /
				  sizeof(ffdhe_2048_priv_key[0]) },
			{ (void *)ffdhe_2048_pub_key,
			  sizeof(ffdhe_2048_pub_key) /
				  sizeof(ffdhe_2048_pub_key[0]) },
			{ (void *)ffdhe_2048_result,
			  sizeof(ffdhe_2048_result) /
				  sizeof(ffdhe_2048_result[0]) },
			{ 0, 0, GNUTLS_FIPS140_OP_INITIAL },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
			{ 0, 0, GNUTLS_FIPS140_OP_APPROVED },
		},
		{ NULL }
	};

	global_init();

	for (int i = 0; test_data[i].name != NULL; i++) {
		gnutls_datum_t priv_key = { NULL, 0 }, pub_key = { NULL, 0 };
		const gnutls_datum_t *q =
			test_data[i].q.data == NULL ? NULL : &test_data[i].q;
		const gnutls_datum_t *result =
			test_data[i].result.data == NULL ? NULL :
							   &test_data[i].result;
		gnutls_dh_params_t dh_params = NULL;
		gnutls_fips140_context_t fips_context;
		int ret;

		if (gnutls_fips140_mode_enabled()) {
			ret = gnutls_fips140_context_init(&fips_context);
			if (ret < 0) {
				fail("Cannot initialize FIPS context\n");
			}
		}

		success("%s params\n", test_data[i].name);

		fips_push_context(fips_context);
		params(&dh_params, &test_data[i].prime, q,
		       &test_data[i].generator,
		       get_expected(&test_data[i].params_expected));
		fips_pop_context(fips_context,
				 test_data[i].params_expected.state);

		if (get_expected(&test_data[i].params_expected) < 0) {
			goto skip;
		}

		if (test_data[i].priv_key.data == NULL) {
			success("%s genkey\n", test_data[i].name);

			fips_push_context(fips_context);
			genkey(dh_params, &priv_key, &pub_key,
			       get_expected(&test_data[i].genkey_expected));
			fips_pop_context(fips_context,
					 test_data[i].genkey_expected.state);

			if (get_expected(&test_data[i].genkey_expected) < 0) {
				goto skip;
			}
		} else {
			priv_key = test_data[i].priv_key;
			pub_key = test_data[i].pub_key;
		}

		success("%s compute_key\n", test_data[i].name);

		fips_push_context(fips_context);
		compute_key(test_data[i].name, dh_params, &priv_key, &pub_key,
			    &test_data[i].peer_key,
			    get_expected(&test_data[i].import_expected),
			    get_expected(&test_data[i].derive_expected),
			    result);

		if (get_expected(&test_data[i].import_expected) < 0) {
			fips_pop_context(fips_context,
					 test_data[i].import_expected.state);
			goto skip;
		}

		if (get_expected(&test_data[i].derive_expected) < 0) {
			fips_pop_context(fips_context,
					 test_data[i].derive_expected.state);
			goto skip;
		}

		fips_pop_context(fips_context,
				 test_data[i].derive_expected.state);

	skip:
		gnutls_dh_params_deinit(dh_params);

		if (test_data[i].priv_key.data == NULL) {
			gnutls_free(priv_key.data);
			gnutls_free(pub_key.data);
		}

		if (gnutls_fips140_mode_enabled()) {
			gnutls_fips140_context_deinit(fips_context);
		}
	}

	success("all ok\n");

	gnutls_global_deinit();
}
