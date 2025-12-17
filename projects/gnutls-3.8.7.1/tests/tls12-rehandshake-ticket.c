/*
 * Copyright (C) 2022 Red Hat, Inc.
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

#include <gnutls/gnutls.h>
#include <assert.h>
#include "cert-common.h"

#include "utils.h"
#include "eagain-common.h"

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

#define MAX_BUF 1024

void _gnutls_session_ticket_disable_server(gnutls_session_t session);

static void run(void)
{
	char buffer[MAX_BUF + 1];
	/* Server stuff. */
	gnutls_certificate_credentials_t scred;
	gnutls_session_t server;
	gnutls_datum_t session_ticket_key = { NULL, 0 };
	int sret;
	/* Client stuff. */
	gnutls_certificate_credentials_t ccred;
	gnutls_session_t client;
	int cret;

	/* General init. */
	global_init();
	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(9);

	/* Init server */
	assert(gnutls_certificate_allocate_credentials(&scred) >= 0);
	assert(gnutls_certificate_set_x509_key_mem(
		       scred, &server_ca3_localhost_cert, &server_ca3_key,
		       GNUTLS_X509_FMT_PEM) >= 0);
	assert(gnutls_certificate_set_x509_trust_mem(scred, &ca3_cert,
						     GNUTLS_X509_FMT_PEM) >= 0);

	assert(gnutls_init(&server, GNUTLS_SERVER) >= 0);
	gnutls_certificate_server_set_request(server, GNUTLS_CERT_REQUEST);
	assert(gnutls_priority_set_direct(
		       server, "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.1:+VERS-TLS1.2",
		       NULL) >= 0);

	gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE, scred);
	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);

	gnutls_session_ticket_key_generate(&session_ticket_key);
	gnutls_session_ticket_enable_server(server, &session_ticket_key);

	/* Init client */
	assert(gnutls_certificate_allocate_credentials(&ccred) >= 0);
	assert(gnutls_certificate_set_x509_key_mem(ccred, &cli_ca3_cert_chain,
						   &cli_ca3_key,
						   GNUTLS_X509_FMT_PEM) >= 0);
	assert(gnutls_certificate_set_x509_trust_mem(ccred, &ca3_cert,
						     GNUTLS_X509_FMT_PEM) >= 0);

	gnutls_init(&client, GNUTLS_CLIENT);
	assert(gnutls_priority_set_direct(
		       client, "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.1:+VERS-TLS1.2",
		       NULL) >= 0);

	assert(gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE, ccred) >=
	       0);

	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);

	HANDSHAKE(client, server);

	/* Server initiates rehandshake */
	switch_side("server");
	sret = gnutls_rehandshake(server);
	if (sret < 0) {
		fail("Error sending %d byte packet: %s\n", (int)sizeof(buffer),
		     gnutls_strerror(sret));
	} else if (debug)
		success("server: starting rehandshake\n");

	/* Stop sending session ticket */
	_gnutls_session_ticket_disable_server(server);

	/* Client gets notified with rehandshake */
	switch_side("client");
	do {
		do {
			cret = gnutls_record_recv(client, buffer, MAX_BUF);
		} while (cret == GNUTLS_E_AGAIN ||
			 cret == GNUTLS_E_INTERRUPTED);
	} while (cret > 0);

	if (cret != GNUTLS_E_REHANDSHAKE) {
		fail("client: Error receiving rehandshake: %s\n",
		     gnutls_strerror(cret));
	}

	HANDSHAKE(client, server);

	gnutls_bye(client, GNUTLS_SHUT_WR);
	gnutls_bye(server, GNUTLS_SHUT_WR);

	gnutls_deinit(client);
	gnutls_deinit(server);

	gnutls_certificate_free_credentials(scred);
	gnutls_certificate_free_credentials(ccred);

	gnutls_free(session_ticket_key.data);

	gnutls_global_deinit();
	reset_buffers();
}

void doit(void)
{
	run();
}
