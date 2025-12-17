/*
 * Copyright (C) 2018 Nikos Mavrogiannopoulos
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>
#include <gnutls/pkcs11.h>
#include <p11-kit/pkcs11.h>

#ifdef _WIN32

void doit(void)
{
	exit(77);
}

#else

#include "../utils.h"
#include "softhsm.h"
#include <assert.h>

#define CONFIG_NAME "softhsm-generate"
#define CONFIG CONFIG_NAME ".config"
#define PIN "1234"
/* Tests whether a gnutls_privkey_generate3 will work generate a key
 * which is marked as sensitive.
 */

static unsigned pin_called = 0;
static const char *_pin = PIN;

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "|<%d>| %s", level, str);
}

static int pin_func(void *userdata, int attempt, const char *url,
		    const char *label, unsigned flags, char *pin,
		    size_t pin_max)
{
	if (_pin == NULL)
		return -1;

	strcpy(pin, _pin);
	pin_called++;
	return 0;
}

static void generate_keypair(gnutls_pk_algorithm_t algo, size_t bits,
			     const char *name, bool sensitive)
{
	gnutls_datum_t out;
	unsigned int flags;
	gnutls_pkcs11_obj_t obj;
	char buf[128];
	int ret;

	flags = GNUTLS_PKCS11_OBJ_FLAG_LOGIN;
	if (!sensitive) {
		flags |= GNUTLS_PKCS11_OBJ_FLAG_MARK_NOT_SENSITIVE;
	}

	ret = gnutls_pkcs11_privkey_generate3("pkcs11:token=test", algo, bits,
					      name, NULL, GNUTLS_X509_FMT_DER,
					      &out, 0, flags);
	if (ret < 0) {
		fail("%d: %s\n", ret, gnutls_strerror(ret));
	}

	success("generated %s key (%s)\n", gnutls_pk_get_name(algo),
		sensitive ? "sensitive" : "non sensitive");

	assert(gnutls_pkcs11_obj_init(&obj) >= 0);
	assert(out.size > 0);

	gnutls_pkcs11_obj_set_pin_function(obj, pin_func, NULL);
	assert(snprintf(buf, sizeof(buf),
			"pkcs11:token=test;object=%s;type=private",
			name) < (int)sizeof(buf));
	assert(gnutls_pkcs11_obj_import_url(obj, buf,
					    GNUTLS_PKCS11_OBJ_FLAG_LOGIN) >= 0);

	assert(gnutls_pkcs11_obj_get_flags(obj, &flags) >= 0);

	if (sensitive) {
		assert(!(flags & GNUTLS_PKCS11_OBJ_FLAG_MARK_NOT_SENSITIVE));
		assert(flags & GNUTLS_PKCS11_OBJ_FLAG_MARK_SENSITIVE);
	} else {
		assert(!(flags & GNUTLS_PKCS11_OBJ_FLAG_MARK_SENSITIVE));
		assert(flags & GNUTLS_PKCS11_OBJ_FLAG_MARK_NOT_SENSITIVE);
	}

	gnutls_free(out.data);
	gnutls_pkcs11_obj_deinit(obj);
}

void doit(void)
{
	char buf[128];
	int ret;
	const char *lib, *bin;
#ifdef CKM_EC_EDWARDS_KEY_PAIR_GEN
	CK_MECHANISM_INFO minfo;
#endif

	if (gnutls_fips140_mode_enabled())
		exit(77);

	ret = global_init();
	if (ret != 0) {
		fail("%d: %s\n", ret, gnutls_strerror(ret));
		exit(1);
	}

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	bin = softhsm_bin();

	lib = softhsm_lib();

	set_softhsm_conf(CONFIG);
	snprintf(buf, sizeof(buf),
		 "%s --init-token --slot 0 --label test --so-pin " PIN
		 " --pin " PIN,
		 bin);
	system(buf);

	ret = gnutls_pkcs11_init(GNUTLS_PKCS11_FLAG_MANUAL, NULL);
	if (ret != 0) {
		fail("%d: %s\n", ret, gnutls_strerror(ret));
		exit(1);
	}

	ret = gnutls_pkcs11_add_provider(lib, NULL);
	if (ret != 0) {
		fail("%d: %s\n", ret, gnutls_strerror(ret));
		exit(1);
	}

	gnutls_pkcs11_set_pin_function(pin_func, NULL);

	generate_keypair(GNUTLS_PK_RSA, 2048, "rsa-sensitive", true);
	generate_keypair(GNUTLS_PK_RSA, 2048, "rsa-non-sensitive", false);

#ifdef CKM_EC_EDWARDS_KEY_PAIR_GEN
	ret = gnutls_pkcs11_token_check_mechanism("pkcs11:token=test",
						  CKM_EC_EDWARDS_KEY_PAIR_GEN,
						  &minfo, sizeof(minfo), 0);
	if (ret != 0) {
		generate_keypair(GNUTLS_PK_EDDSA_ED25519, 256,
				 "ed25519-sensitive", true);
		generate_keypair(GNUTLS_PK_EDDSA_ED25519, 256,
				 "ed25519-non-sensitive", false);
		if (minfo.ulMaxKeySize >= 456) {
			generate_keypair(GNUTLS_PK_EDDSA_ED448, 456,
					 "ed448-sensitive", true);
			generate_keypair(GNUTLS_PK_EDDSA_ED448, 456,
					 "ed448-non-sensitive", false);
		}
	}
#endif

	gnutls_pkcs11_deinit();
	gnutls_global_deinit();
	remove(CONFIG);
}
#endif
