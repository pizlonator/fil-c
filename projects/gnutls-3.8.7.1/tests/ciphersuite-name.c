/*
 * Copyright (C) 2022 Red Hat, Inc.
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

/* This tests gnutls_cipher_suite_get() and
 * gnutls_cipher_suite_get_canonical_name()
 */

#include "config.h"

#include <gnutls/gnutls.h>

#include <string.h>
#include "cert-common.h"
#include "eagain-common.h"
#include "utils.h"

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

static void start(const char *test_name, const char *prio,
		  const char *expected_name)
{
	int sret, cret;
	gnutls_certificate_credentials_t scred, ccred;
	gnutls_session_t server, client;
	const char *name;

	success("%s\n", test_name);

	global_init();
	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(9);

	/* Init server */
	assert(gnutls_certificate_allocate_credentials(&scred) >= 0);
	assert(gnutls_certificate_set_x509_key_mem(scred, &server_cert,
						   &server_key,
						   GNUTLS_X509_FMT_PEM) >= 0);

	gnutls_init(&server, GNUTLS_SERVER);

	gnutls_priority_set_direct(server, prio, NULL);
	gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE, scred);
	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);

	/* Init client */
	gnutls_certificate_allocate_credentials(&ccred);
	assert(gnutls_certificate_set_x509_trust_mem(ccred, &ca3_cert,
						     GNUTLS_X509_FMT_PEM) >= 0);

	gnutls_init(&client, GNUTLS_CLIENT);

	gnutls_priority_set_direct(client, prio, NULL);
	gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE, ccred);
	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);

	HANDSHAKE(client, server);
	if (debug)
		success("Handshake established\n");

	name = gnutls_ciphersuite_get(server);
	if (!name || strcmp(name, expected_name) != 0) {
		fail("server: gnutls_ciphersuite_get returned %s while %s is expected\n",
		     name, expected_name);
	}

	name = gnutls_ciphersuite_get(client);
	if (!name || strcmp(name, expected_name) != 0) {
		fail("client: gnutls_ciphersuite_get returned %s while %s is expected\n",
		     name, expected_name);
	}

	gnutls_bye(client, GNUTLS_SHUT_WR);
	gnutls_bye(server, GNUTLS_SHUT_WR);

	gnutls_deinit(client);
	gnutls_deinit(server);

	gnutls_certificate_free_credentials(scred);
	gnutls_certificate_free_credentials(ccred);

	gnutls_global_deinit();
	reset_buffers();
}

void doit(void)
{
	start("TLS 1.3 name",
	      "NONE:+VERS-TLS1.3:+AES-256-GCM:+AEAD:+SIGN-ALL:+GROUP-ALL",
	      "TLS_AES_256_GCM_SHA384");

	start("TLS 1.2 name",
	      "NONE:+VERS-TLS1.2:+AES-128-GCM:+MAC-ALL:+SIGN-ALL:+RSA",
	      "TLS_RSA_WITH_AES_128_GCM_SHA256");
}
