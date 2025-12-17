/*
 * Copyright (C) 2020 Red Hat, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gnutls/gnutls.h>
#include <assert.h>
#include "cert-common.h"

#include "utils.h"
#define RANDOMIZE
#include "eagain-common.h"

/* This tests gnutls_record_set_write_function() and
 * gnutls_record_push_data() by short-circuiting the handshake.
 */

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

#define MAX_BUF 1024
#define MSG \
	"Hello TLS, and hi and how are you and more data here... and more... and even more and even more more data..."

static ssize_t error_push(gnutls_transport_ptr_t tr, const void *data,
			  size_t len)
{
	fail("push_func called unexpectedly");
	return -1;
}

static ssize_t error_pull(gnutls_transport_ptr_t tr, void *data, size_t len)
{
	fail("pull_func called unexpectedly");
	return -1;
}

static int handshake_read_func(gnutls_session_t session,
			       gnutls_record_encryption_level_t level,
			       gnutls_handshake_description_t htype,
			       const void *data, size_t data_size)
{
	gnutls_session_t peer = gnutls_session_get_ptr(session);

	if (htype == GNUTLS_HANDSHAKE_CHANGE_CIPHER_SPEC)
		return 0;

	return gnutls_handshake_write(peer, level, data, data_size);
}

static void run(const char *name, const char *prio)
{
	/* Server stuff. */
	gnutls_certificate_credentials_t scred;
	gnutls_session_t server;
	/* Client stuff. */
	gnutls_certificate_credentials_t ccred;
	gnutls_session_t client;
	int sret, cret;
	char buffer[MAX_BUF + 1];
	int transferred = 0;

	success("%s\n", name);

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

	assert(gnutls_init(&server, GNUTLS_SERVER) >= 0);
	assert(gnutls_priority_set_direct(server, prio, NULL) >= 0);

	gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE, scred);
	gnutls_transport_set_push_function(server, error_push);
	gnutls_transport_set_pull_function(server, error_pull);

	/* Init client */
	assert(gnutls_certificate_allocate_credentials(&ccred) >= 0);
	assert(gnutls_certificate_set_x509_trust_mem(ccred, &ca3_cert,
						     GNUTLS_X509_FMT_PEM) >= 0);

	gnutls_init(&client, GNUTLS_CLIENT);
	assert(gnutls_priority_set_direct(client, prio, NULL) >= 0);

	assert(gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE, ccred) >=
	       0);

	gnutls_transport_set_push_function(client, error_push);
	gnutls_transport_set_pull_function(client, error_pull);

	gnutls_session_set_ptr(server, client);
	gnutls_session_set_ptr(client, server);

	gnutls_handshake_set_read_function(server, handshake_read_func);
	gnutls_handshake_set_read_function(client, handshake_read_func);

	HANDSHAKE(client, server);
	if (debug)
		success("Handshake established\n");

	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);

	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);

	TRANSFER(client, server, MSG, strlen(MSG), buffer, MAX_BUF);
	TRANSFER(server, client, MSG, strlen(MSG), buffer, MAX_BUF);

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
	run("TLS 1.3", "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.3");
}
