/*
 * Copyright (C) 2004-2012 Free Software Foundation, Inc.
 * Copyright (C) 2013 Adam Sampson <ats@offog.org>
 * Copyright (C) 2019 Free Software Foundation, Inc.
 * Copyright (C) 2023 Red Hat, Inc.
 *
 * Author: Simon Josefsson, Ander Juaristi, Daiki Ueno
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

/* This tests the external PSK importer interface (RFC 9258).  */
/* Parts copied from pskself.c.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <gnutls/gnutls.h>

#include "utils.h"
#include "eagain-common.h"

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

#define MAX_BUF 1024
#define MSG "Hello TLS"

static const gnutls_datum_t identity = { (unsigned char *)"\xCA\xFE\xCA\xFE",
					 4 };
static const gnutls_datum_t context = { (unsigned char *)"\xDE\xAD\xBE\xEF",
					4 };

static const uint8_t expected_imported_identity[] = { 0x00, 0x04, 0xca, 0xfe,
						      0xca, 0xfe, 0x00, 0x04,
						      0xde, 0xad, 0xbe, 0xef,
						      0x03, 0x04, 0x00, 0x01 };

static int server_pskfunc(gnutls_session_t session,
			  const gnutls_datum_t *username, gnutls_datum_t *key,
			  gnutls_psk_key_flags *flags)
{
	gnutls_datum_t imported_identity = { NULL, 0 };

	int ret;

	if (debug)
		printf("psk: Got username with length %d\n", username->size);

	key->data = gnutls_malloc(4);
	key->data[0] = 0xDE;
	key->data[1] = 0xAD;
	key->data[2] = 0xBE;
	key->data[3] = 0xEF;
	key->size = 4;

	ret = gnutls_psk_format_imported_identity(&identity, &context,
						  GNUTLS_TLS1_3,
						  GNUTLS_DIG_SHA256,
						  &imported_identity);
	if (ret < 0) {
		return -1;
	}

	if (imported_identity.size != sizeof(expected_imported_identity) ||
	    memcmp(imported_identity.data, expected_imported_identity,
		   imported_identity.size)) {
		gnutls_free(imported_identity.data);
		printf("Unexpected imported identity\n");
		return -1;
	}
	gnutls_free(imported_identity.data);

	if (flags) {
		*flags = GNUTLS_PSK_KEY_EXT;
	}

	return 0;
}

static int client_pskfunc(gnutls_session_t session, gnutls_datum_t *username,
			  gnutls_datum_t *key, gnutls_psk_key_flags *flags)
{
	int ret;

	ret = gnutls_psk_format_imported_identity(&identity, &context,
						  GNUTLS_TLS1_3,
						  GNUTLS_DIG_SHA256, username);
	if (ret < 0) {
		return -1;
	}

	key->data = gnutls_malloc(4);
	key->data[0] = 0xDE;
	key->data[1] = 0xAD;
	key->data[2] = 0xBE;
	key->data[3] = 0xEF;
	key->size = 4;

	*flags = GNUTLS_PSK_KEY_EXT;

	return 0;
}

static void run_test(const char *prio)
{
	/* Server stuff. */
	gnutls_psk_server_credentials_t server_pskcred;
	gnutls_session_t server;
	int sret;

	/* Client stuff. */
	gnutls_psk_client_credentials_t client_pskcred;
	gnutls_session_t client;
	int cret;

	char buffer[MAX_BUF + 1];
	int transferred = 0;

	gnutls_datum_t psk_username;

	success("%s\n", prio);

	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(4711);

	/* Init server */
	gnutls_psk_allocate_server_credentials(&server_pskcred);
	gnutls_psk_set_server_credentials_function3(server_pskcred,
						    server_pskfunc);
	gnutls_init(&server, GNUTLS_SERVER);
	gnutls_priority_set_direct(server, prio, NULL);
	gnutls_credentials_set(server, GNUTLS_CRD_PSK, server_pskcred);
	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);

	/* Init client */
	gnutls_psk_allocate_client_credentials(&client_pskcred);
	gnutls_psk_set_client_credentials_function3(client_pskcred,
						    client_pskfunc);
	gnutls_init(&client, GNUTLS_CLIENT);
	gnutls_priority_set_direct(client, prio, NULL);
	gnutls_credentials_set(client, GNUTLS_CRD_PSK, client_pskcred);
	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);

	HANDSHAKE(client, server);

	if (debug) {
		success("server: Handshake was completed\n");

		if (gnutls_psk_server_get_username(server))
			fail("server: gnutls_psk_server_get_username() should have returned NULL\n");
		if (gnutls_psk_server_get_username2(server, &psk_username) < 0)
			fail("server: Could not get PSK username\n");

		if (psk_username.size != sizeof(expected_imported_identity) ||
		    memcmp(psk_username.data, expected_imported_identity,
			   sizeof(expected_imported_identity)))
			fail("server: Unexpected PSK username\n");

		success("server: PSK username length: %d\n", psk_username.size);
	}

	TRANSFER(client, server, MSG, strlen(MSG), buffer, MAX_BUF);
	TRANSFER(server, client, MSG, strlen(MSG), buffer, MAX_BUF);

	gnutls_bye(server, GNUTLS_SHUT_WR);
	gnutls_bye(client, GNUTLS_SHUT_WR);

	gnutls_deinit(server);
	gnutls_deinit(client);

	gnutls_psk_free_server_credentials(server_pskcred);
	gnutls_psk_free_client_credentials(client_pskcred);

	reset_buffers();
}

void doit(void)
{
	global_init();
	run_test("NORMAL:-VERS-ALL:+VERS-TLS1.3:+PSK");
	run_test(
		"NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-FFDHE2048:+DHE-PSK");
	run_test(
		"NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-SECP256R1:+ECDHE-PSK");

	gnutls_global_deinit();
}
