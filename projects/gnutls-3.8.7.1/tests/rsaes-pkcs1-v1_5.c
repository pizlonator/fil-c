/*
 * Copyright (C) 2024 Red Hat, Inc.
 *
 * Author: Zoltan Fridrich
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <gnutls/abstract.h>
#include <gnutls/gnutls.h>

static const char pem[] =
	"-----BEGIN RSA PRIVATE KEY-----\n"
	"MIICXAIBAAKBgQC7ZkP18sXXtozMxd/1iDuxyUtqDqGtIFBACIChT1yj0Phsz+Y8\n"
	"9+wEdhMXi2SJIlvA3VN8O+18BLuAuSi+jpvGjqClEsv1Vx6i57u3M0mf47tKrmpN\n"
	"aP/JEeIyjc49gAuNde/YAIGPKAQDoCKNYQQH+rY3fSEHSdIJYWmYkKNYqQIDAQAB\n"
	"AoGADpmARG5CQxS+AesNkGmpauepiCz1JBF/JwnyiX6vEzUh0Ypd39SZztwrDxvF\n"
	"PJjQaKVljml1zkJpIDVsqvHdyVdse8M+Qn6hw4x2p5rogdvhhIL1mdWo7jWeVJTF\n"
	"RKB7zLdMPs3ySdtcIQaF9nUAQ2KJEvldkO3m/bRJFEp54k0CQQDYy+RlTmwRD6hy\n"
	"7UtMjR0H3CSZJeQ8svMCxHLmOluG9H1UKk55ZBYfRTsXniqUkJBZ5wuV1L+pR9EK\n"
	"ca89a+1VAkEA3UmBelwEv2u9cAU1QjKjmwju1JgXbrjEohK+3B5y0ESEXPAwNQT9\n"
	"TrDM1m9AyxYTWLxX93dI5QwNFJtmbtjeBQJARSCWXhsoaDRG8QZrCSjBxfzTCqZD\n"
	"ZXtl807ymCipgJm60LiAt0JLr4LiucAsMZz6+j+quQbSakbFCACB8SLV1QJBAKZQ\n"
	"YKf+EPNtnmta/rRKKvySsi3GQZZN+Dt3q0r094XgeTsAqrqujVNfPhTMeP4qEVBX\n"
	"/iVX2cmMTSh3w3z8MaECQEp0XJWDVKOwcTW6Ajp9SowtmiZ3YDYo1LF9igb4iaLv\n"
	"sWZGfbnU3ryjvkb6YuFjgtzbZDZHWQCo8/cOtOBmPdk=\n"
	"-----END RSA PRIVATE KEY-----\n";

const gnutls_datum_t privkey_datum = { (void *)pem, sizeof(pem) };
const gnutls_datum_t in_message = { (void *)"hello", 5 };

int main(void)
{
	int ret = EXIT_FAILURE;
	gnutls_datum_t crypt = { NULL, 0 };
	gnutls_datum_t out_message = { NULL, 0 };
	gnutls_pubkey_t pubkey = NULL;
	gnutls_privkey_t privkey = NULL;

	gnutls_global_init();
	assert(gnutls_privkey_init(&privkey) >= 0);
	assert(gnutls_pubkey_init(&pubkey) >= 0);
	assert(gnutls_privkey_import_x509_raw(privkey, &privkey_datum,
					      GNUTLS_X509_FMT_PEM, NULL,
					      0) >= 0);
	assert(gnutls_pubkey_import_privkey(pubkey, privkey, 0, 0) >= 0);

	if (gnutls_pubkey_encrypt_data(pubkey, 0, &in_message, &crypt) < 0 ||
	    gnutls_privkey_decrypt_data(privkey, 0, &crypt, &out_message) < 0)
		goto cleanup;

	assert(in_message.size == out_message.size);
	assert(memcmp(out_message.data, in_message.data, in_message.size) == 0);
	ret = EXIT_SUCCESS;
cleanup:
	gnutls_free(crypt.data);
	gnutls_free(out_message.data);
	gnutls_privkey_deinit(privkey);
	gnutls_pubkey_deinit(pubkey);
	gnutls_global_deinit();
	return ret;
}
