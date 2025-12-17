/*
 * Copyright (C) 2020 Red Hat, Inc.
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

#include "config.h"

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "utils.h"

#define MAX_BUF 1024

static gnutls_fips140_context_t fips_context;

static void test_hkdf(gnutls_mac_algorithm_t mac, const char *ikm_hex,
		      const char *salt_hex, const char *info_hex, size_t length,
		      const char *prk_hex, const char *okm_hex)
{
	gnutls_datum_t hex;
	gnutls_datum_t ikm;
	gnutls_datum_t salt;
	gnutls_datum_t info;
	gnutls_datum_t prk;
	gnutls_datum_t okm;
	uint8_t buf[MAX_BUF];

	success("HKDF test with %s\n", gnutls_mac_get_name(mac));

	/* Test HKDF-Extract */
	hex.data = (void *)ikm_hex;
	hex.size = strlen(ikm_hex);
	assert(gnutls_hex_decode2(&hex, &ikm) >= 0);

	hex.data = (void *)salt_hex;
	hex.size = strlen(salt_hex);
	assert(gnutls_hex_decode2(&hex, &salt) >= 0);

	FIPS_PUSH_CONTEXT();
	assert(gnutls_hkdf_extract(mac, &ikm, &salt, buf) >= 0);
	/* HKDF outside of TLS usage is not approved */
	FIPS_POP_CONTEXT(NOT_APPROVED);
	gnutls_free(ikm.data);
	gnutls_free(salt.data);

	prk.data = buf;
	prk.size = strlen(prk_hex) / 2;
	assert(gnutls_hex_encode2(&prk, &hex) >= 0);

	if (strcmp((char *)hex.data, prk_hex))
		fail("prk doesn't match: %s != %s\n", (char *)hex.data,
		     prk_hex);

	gnutls_free(hex.data);

	/* Test HKDF-Expand */
	hex.data = (void *)info_hex;
	hex.size = strlen(info_hex);
	assert(gnutls_hex_decode2(&hex, &info) >= 0);

	FIPS_PUSH_CONTEXT();
	assert(gnutls_hkdf_expand(mac, &prk, &info, buf,
				  gnutls_hmac_get_len(mac) * 256) ==
	       GNUTLS_E_INVALID_REQUEST);
	FIPS_POP_CONTEXT(ERROR);

	FIPS_PUSH_CONTEXT();
	assert(gnutls_hkdf_expand(mac, &prk, &info, buf, length) >= 0);
	/* HKDF outside of TLS usage is not approved */
	FIPS_POP_CONTEXT(NOT_APPROVED);

	gnutls_free(info.data);

	okm.data = buf;
	okm.size = strlen(okm_hex) / 2;
	assert(gnutls_hex_encode2(&okm, &hex) >= 0);

	if (strcmp((char *)hex.data, okm_hex))
		fail("okm doesn't match: %s != %s\n", (char *)hex.data,
		     okm_hex);

	gnutls_free(hex.data);
}

inline static bool
is_mac_algo_hmac_approved_in_fips(gnutls_mac_algorithm_t algo)
{
	switch (algo) {
	case GNUTLS_MAC_SHA1:
	case GNUTLS_MAC_SHA256:
	case GNUTLS_MAC_SHA384:
	case GNUTLS_MAC_SHA512:
	case GNUTLS_MAC_SHA224:
	case GNUTLS_MAC_SHA3_224:
	case GNUTLS_MAC_SHA3_256:
	case GNUTLS_MAC_SHA3_384:
	case GNUTLS_MAC_SHA3_512:
		return true;
	default:
		return false;
	}
}

static void test_pbkdf2(gnutls_mac_algorithm_t mac, const char *ikm_hex,
			const char *salt_hex, unsigned iter_count,
			size_t length, const char *okm_hex,
			gnutls_fips140_operation_state_t expected_state)
{
	gnutls_datum_t hex;
	gnutls_datum_t ikm;
	gnutls_datum_t salt;
	gnutls_datum_t okm;
	uint8_t buf[MAX_BUF];

	success("PBKDF2 test with %s\n", gnutls_mac_get_name(mac));

	hex.data = (void *)ikm_hex;
	hex.size = strlen(ikm_hex);
	assert(gnutls_hex_decode2(&hex, &ikm) >= 0);

	hex.data = (void *)salt_hex;
	hex.size = strlen(salt_hex);
	assert(gnutls_hex_decode2(&hex, &salt) >= 0);

	fips_push_context(fips_context);
	assert(gnutls_pbkdf2(mac, &ikm, &salt, iter_count, buf, length) >= 0);
	fips_pop_context(fips_context, expected_state);
	gnutls_free(ikm.data);
	gnutls_free(salt.data);

	okm.data = buf;
	okm.size = length;
	assert(gnutls_hex_encode2(&okm, &hex) >= 0);

	if (strcmp((char *)hex.data, okm_hex))
		fail("okm doesn't match: %s != %s\n", (char *)hex.data,
		     okm_hex);

	gnutls_free(hex.data);
}

void doit(void)
{
	assert(gnutls_fips140_context_init(&fips_context) >= 0);

	/* Test vector from RFC 5869.  More thorough testing is done
	 * in nettle. */
	test_hkdf(GNUTLS_MAC_SHA256,
		  "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"
		  "0b0b0b0b0b0b",
		  "000102030405060708090a0b0c", "f0f1f2f3f4f5f6f7f8f9", 42,
		  "077709362c2e32df0ddc3f0dc47bba63"
		  "90b6c73bb50f9c3122ec844ad7c2b3e5",
		  "3cb25f25faacd57a90434f64d0362f2a"
		  "2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
		  "34007208d5b887185865");

	/* Test vector from RFC 6070.  More thorough testing is done
	 * in nettle. */
	test_pbkdf2(
		GNUTLS_MAC_SHA1, "70617373776f7264", /* "password" */
		"73616c74", /* "salt" */
		4096, 20, "4b007901b765489abead49d926f721d065a429c1",
		/* Key sizes and output sizes less than 112-bit are not approved.  */
		GNUTLS_FIPS140_OP_NOT_APPROVED);

	test_pbkdf2(GNUTLS_MAC_AES_CMAC_128,
		    "70617373776f726470617373776f7264", /* "passwordpassword" */
		    "73616c74", /* "salt" */
		    4096, 20, "c4c112c6e1e3b8757640603dec78825ff87605a7",
		    /* Use of AES-CMAC in PBKDF2 is not supported in ACVP.  */
		    GNUTLS_FIPS140_OP_NOT_APPROVED);

	gnutls_fips140_context_deinit(fips_context);
}
