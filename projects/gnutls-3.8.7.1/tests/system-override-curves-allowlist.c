/*
 * Copyright (C) 2021 Red Hat, Inc.
 *
 * Author: Alexander Sosedkin
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

/*
 * This test isn't meant for direct execution.
 * It is the significant part of the test
 * invoked from system-override-curves-allowlist.sh that covers:
 * - generating a key using the curve enabled in the config succeeds
 * - disabling the previously enabled curve results in blocking it
 * - reenabling it back is also possible after disabling
 * - enabling a different curve unblocks key generation using it
 * - disabling the originally enabled curve results in blocking it
 * - redisabling it back is also possible after enabling
 * Inputs (passed through environment variables):
 * - INITIALLY_ENABLED_CURVES  - space-separated string
 * - INITIALLY_DISABLED_CURVES - space-separated string
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>

#include "utils.h"

#define _assert(cond, format, ...) \
	if (!(cond))               \
	_fail("Assertion `" #cond "` failed: " format "\n", ##__VA_ARGS__)
#define _check(cond) \
	if (!(cond)) \
	_fail("Assertion `" #cond "` failed.\n")

gnutls_ecc_curve_t unlocked_ecc_curve_get_id(const char *curve);
gnutls_pk_algorithm_t curve_name_to_pk(const char *curve);
void assert_unblocked(const char *curve);
void assert_blocked(const char *curve);
char *envvarcpy(const char *envvar);

gnutls_ecc_curve_t unlocked_ecc_curve_get_id(const char *curve)
{
	if (!strcasecmp(curve, "SECP192R1"))
		return GNUTLS_ECC_CURVE_SECP192R1;
	if (!strcasecmp(curve, "SECP256R1"))
		return GNUTLS_ECC_CURVE_SECP256R1;
	if (!strcasecmp(curve, "SECP384R1"))
		return GNUTLS_ECC_CURVE_SECP384R1;
	if (!strcasecmp(curve, "SECP521R1"))
		return GNUTLS_ECC_CURVE_SECP521R1;
	if (!strcasecmp(curve, "X448"))
		return GNUTLS_ECC_CURVE_X448;
	if (!strcasecmp(curve, "X25519"))
		return GNUTLS_ECC_CURVE_X25519;
	fprintf(stderr, "unknown curve %s\n", curve);
	return GNUTLS_ECC_CURVE_INVALID;
}

gnutls_pk_algorithm_t curve_name_to_pk(const char *curve)
{
	if (!strcasecmp(curve, "X448"))
		return GNUTLS_PK_ECDH_X448;
	if (!strcasecmp(curve, "X25519"))
		return GNUTLS_PK_ECDH_X25519;
	return GNUTLS_PK_ECDSA;
}

void assert_unblocked(const char *curve_name)
{
	gnutls_privkey_t priv;
	gnutls_ecc_curve_t curve;
	gnutls_pk_algorithm_t pk;

	unsigned int bits;

	fprintf(stderr, "generating a key using non-blocked %s curve...\n",
		curve_name);
	_check(curve = gnutls_ecc_curve_get_id(curve_name));
	_check(curve == unlocked_ecc_curve_get_id(curve_name));
	_check(gnutls_privkey_init(&priv) == GNUTLS_E_SUCCESS);
	bits = GNUTLS_CURVE_TO_BITS(curve);
	pk = curve_name_to_pk(curve_name);
	_check(gnutls_privkey_generate(priv, pk, bits, 0) == GNUTLS_E_SUCCESS);
	gnutls_privkey_deinit(priv);
	fprintf(stderr, "%s succeeds as expected\n", curve_name);
}

void assert_blocked(const char *curve_name)
{
	gnutls_privkey_t priv;
	gnutls_ecc_curve_t curve;
	unsigned int bits;
	gnutls_pk_algorithm_t pk;

	fprintf(stderr, "generating a key using blocked %s curve...\n",
		curve_name);
	_check(gnutls_ecc_curve_get_id(curve_name) == GNUTLS_ECC_CURVE_INVALID);
	_check((curve = unlocked_ecc_curve_get_id(curve_name)) !=
	       GNUTLS_ECC_CURVE_INVALID);
	_check(!strcasecmp(curve_name, gnutls_ecc_curve_get_name(curve)));
	_check(gnutls_privkey_init(&priv) == GNUTLS_E_SUCCESS);
	bits = GNUTLS_CURVE_TO_BITS(curve);
	pk = curve_name_to_pk(curve_name);
	_check(gnutls_privkey_generate(priv, pk, bits, 0) < 0);
	gnutls_privkey_deinit(priv);
	fprintf(stderr, "%s is blocked as expected\n", curve_name);
}

char *envvarcpy(const char *envvar)
{
	char *s;
	_assert(s = getenv(envvar), "variable %s is not set", envvar);
	return gnutls_strdup(s);
}

void doit(void)
{
	char *curves;
	const char *curve;
	gnutls_ecc_curve_t curve_id;

	curves = envvarcpy("INITIALLY_ENABLED_CURVES");
	for (curve = strtok(curves, " "); curve; curve = strtok(NULL, " ")) {
		curve_id = unlocked_ecc_curve_get_id(curve);

		assert_unblocked(curve);

		gnutls_ecc_curve_set_enabled(curve_id, 0);
		assert_blocked(curve);

		gnutls_ecc_curve_set_enabled(curve_id, 1);
		assert_unblocked(curve);

		printf("disableable: %s\n", curve);
	}
	free(curves);

	curves = envvarcpy("INITIALLY_DISABLED_CURVES");
	for (curve = strtok(curves, " "); curve; curve = strtok(NULL, " ")) {
		curve_id = unlocked_ecc_curve_get_id(curve);

		assert_blocked(curve);

		gnutls_ecc_curve_set_enabled(curve_id, 1);
		assert_unblocked(curve);

		gnutls_ecc_curve_set_enabled(curve_id, 0);
		assert_blocked(curve);

		printf("reenableable: %s\n", curve);
	}
	free(curves);

	exit(0);
}
