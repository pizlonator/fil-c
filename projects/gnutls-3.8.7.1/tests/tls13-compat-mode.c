/*
 * Copyright (C) 2021 Free Software Foundation, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "cert-common.h"
#include "eagain-common.h"
#include "utils.h"

/* This tests TLS 1.3 middlebox compatibility mode. */

#define COMPAT_PRIO "NORMAL:-VERS-ALL:+VERS-TLS1.3"
#define NO_COMPAT_PRIO COMPAT_PRIO ":%DISABLE_TLS13_COMPAT_MODE"

#define HANDSHAKE_SESSION_ID_POS 34

struct data {
	bool compat;
};

static int handshake_callback(gnutls_session_t session, unsigned int htype,
			      unsigned post, unsigned int incoming,
			      const gnutls_datum_t *msg)
{
	unsigned pos;
	struct data *data;
	uint8_t s;

	assert(htype == GNUTLS_HANDSHAKE_CLIENT_HELLO);
	assert(msg->size >= HANDSHAKE_SESSION_ID_POS);

	data = gnutls_session_get_ptr(session);

	pos = HANDSHAKE_SESSION_ID_POS;
	if (pos + 1 > msg->size)
		fail("error\n");
	s = msg->data[pos];

	if (data->compat && s == 0) {
		fail("empty session ID while compat mode is enabled\n");
	} else if (!data->compat && s > 0) {
		fail("non-empty session ID while compat mode is disabled\n");
	}

	return 0;
}

static void test(const char *name, bool client_compat, bool server_compat)
{
	/* Server stuff. */
	gnutls_certificate_credentials_t serverx509cred;
	gnutls_session_t server;
	int sret = GNUTLS_E_AGAIN;
	/* Client stuff. */
	gnutls_certificate_credentials_t clientx509cred;
	gnutls_session_t client;
	struct data data;
	int cret = GNUTLS_E_AGAIN;

	success("%s\n", name);

	/* Init server */
	assert(gnutls_certificate_allocate_credentials(&serverx509cred) >= 0);
	gnutls_certificate_set_x509_key_mem(serverx509cred, &server_cert,
					    &server_key, GNUTLS_X509_FMT_PEM);

	assert(gnutls_init(&server, GNUTLS_SERVER) >= 0);
	assert(gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE,
				      serverx509cred) >= 0);
	assert(gnutls_priority_set_direct(
		       server, server_compat ? COMPAT_PRIO : NO_COMPAT_PRIO,
		       NULL) >= 0);
	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);

	/* Init client */
	assert(gnutls_certificate_allocate_credentials(&clientx509cred) >= 0);
	assert(gnutls_init(&client, GNUTLS_CLIENT) >= 0);
	assert(gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE,
				      clientx509cred) >= 0);

	assert(gnutls_priority_set_direct(
		       client, client_compat ? COMPAT_PRIO : NO_COMPAT_PRIO,
		       NULL) >= 0);
	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);
	data.compat = client_compat;
	gnutls_session_set_ptr(client, &data);
	gnutls_handshake_set_hook_function(client,
					   GNUTLS_HANDSHAKE_CLIENT_HELLO,
					   GNUTLS_HOOK_POST,
					   handshake_callback);

	HANDSHAKE(client, server);

	gnutls_deinit(client);
	gnutls_deinit(server);

	gnutls_certificate_free_credentials(serverx509cred);
	gnutls_certificate_free_credentials(clientx509cred);
}

void doit(void)
{
	global_init();

	test("client compat, server compat", true, true);
	test("client compat, server non-compat", true, false);
	test("client non-compat, server compat", false, true);
	test("client non-compat, server non-compat", false, false);

	gnutls_global_deinit();
}
