/*
 * Copyright (C) 2017-2022 Red Hat, Inc.
 *
 * Author: Nikos Mavrogiannopoulos, Daiki Ueno
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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gnutls/gnutls.h>

#include "cert-common.h"
#include "utils.h"
#include "tls13/ext-parse.h"
#include "eagain-common.h"

/* This program tests the scenario described at:
 * https://gitlab.com/gnutls/gnutls/-/issues/1303
 *
 * - the server only supports one mode
 * - the client follows server's preference with %SERVER_PRECEDENCE
 * - the client provides two modes, but the first one is not the one the server
 *   supports
 *
 * Previously the server was not able to enable PSK (and thus session
 * resumption) at all in this case and didn't send NewSessionTicket.
 */

const char *testname = "";

#define myfail(fmt, ...) fail("%s: " fmt, testname, ##__VA_ARGS__)

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

static int new_session_ticket_callback(gnutls_session_t session,
				       unsigned int htype, unsigned post,
				       unsigned int incoming,
				       const gnutls_datum_t *msg)
{
	bool *new_session_ticket_sent = gnutls_session_get_ptr(session);
	*new_session_ticket_sent = true;
	return 0;
}

#define MAX_BUF 1024
#define MSG \
	"Hello TLS, and hi and how are you and more data here... and more... and even more and even more more data..."

static void start(const char *name, const char *prio, const char *sprio)
{
	int sret, cret;
	gnutls_certificate_credentials_t scred, ccred;
	gnutls_session_t server, client;
	gnutls_datum_t skey;
	char buffer[MAX_BUF + 1];
	int transferred = 0;
	bool new_session_ticket_sent = false;

	testname = name;
	success("== test %s ==\n", testname);

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

	gnutls_handshake_set_hook_function(server,
					   GNUTLS_HANDSHAKE_NEW_SESSION_TICKET,
					   GNUTLS_HOOK_POST,
					   new_session_ticket_callback);
	gnutls_session_set_ptr(server, &new_session_ticket_sent);

	gnutls_priority_set_direct(server, sprio, NULL);

	assert(gnutls_session_ticket_key_generate(&skey) >= 0);
	assert(gnutls_session_ticket_enable_server(server, &skey) >= 0);

	gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE, scred);
	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);

	/* Init client */
	gnutls_certificate_allocate_credentials(&ccred);
	assert(gnutls_certificate_set_x509_trust_mem(ccred, &ca3_cert,
						     GNUTLS_X509_FMT_PEM) >= 0);

	gnutls_init(&client, GNUTLS_CLIENT);

	cret = gnutls_priority_set_direct(client, prio, NULL);
	if (cret < 0)
		myfail("cannot set TLS 1.3 priorities\n");

	/* put the anonymous credentials to the current session
	 */
	gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE, ccred);

	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);

	HANDSHAKE(client, server);
	if (debug)
		success("Handshake established\n");

	TRANSFER(client, server, MSG, strlen(MSG), buffer, MAX_BUF);
	TRANSFER(server, client, MSG, strlen(MSG), buffer, MAX_BUF);
	EMPTY_BUF(server, client, buffer, MAX_BUF);

	if (!new_session_ticket_sent) {
		fail("NewSessionTicket is not sent\n");
	}

	gnutls_bye(client, GNUTLS_SHUT_WR);
	gnutls_bye(server, GNUTLS_SHUT_WR);

	gnutls_deinit(client);
	gnutls_deinit(server);

	gnutls_certificate_free_credentials(scred);
	gnutls_certificate_free_credentials(ccred);

	gnutls_free(skey.data);

	gnutls_global_deinit();
	reset_buffers();
}

void doit(void)
{
	start("server only supports PSK, client advertises ECDHE-PSK first",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:+ECDHE-PSK:+PSK:%SERVER_PRECEDENCE",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:+PSK");
}
