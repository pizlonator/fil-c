/*
 * Copyright (C) 2021 Ruslan N. Marchenko
 *
 * Author: Ruslan N. Marchenko
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
#include "utils.h"
#include "eagain-common.h"
#include "cert-common.h"

/* This program tests tls channel binding API under TLS 1.3.
 * tls-unique is expected to fail while server-end-point and
 * exporter should succeed and provide same binding data on
 * both ends. Except that server-end-point is only valid for
 * X.509 certificates, thus fails for other types.
 */

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

static int check_binding_data(gnutls_session_t client, gnutls_session_t server,
			      int cbtype, const char *cbname, int negative)
{
	gnutls_datum_t client_cb = { 0 };
	gnutls_datum_t server_cb = { 0 };

	if (gnutls_session_channel_binding(client, cbtype, &client_cb) !=
	    GNUTLS_E_SUCCESS) {
		if (negative == 0) {
			fail("Cannot get client binding %s\n", cbname);
			return 1;
		}
	} else if (negative) {
		fail("Client retrieval of %s was supposed to fail\n", cbname);
		return 1;
	}
	if (gnutls_session_channel_binding(server, cbtype, &server_cb) !=
	    GNUTLS_E_SUCCESS) {
		if (negative == 0) {
			fail("Cannot get server binding %s\n", cbname);
			return -1;
		}
	} else if (negative) {
		fail("Server retrieval of %s was supposed to fail\n", cbname);
		return -1;
	}
	/* If we are here with negative 1 - we're done for now */
	if (negative == 1)
		return 0;

	if (server_cb.size != client_cb.size && client_cb.size > 0) {
		fail("%s wrong binding data length: %d:%d\n", cbname,
		     client_cb.size, server_cb.size);
		return 2;
	}
	if (gnutls_memcmp(client_cb.data, server_cb.data, client_cb.size) !=
	    0) {
		fail("%s wrong binding data content\n", cbname);
		return -2;
	}
	gnutls_free(client_cb.data);
	gnutls_free(server_cb.data);
	return 0;
}

static int serv_psk_func(gnutls_session_t session, const char *user,
			 gnutls_datum_t *pass)
{
	pass->size = 4;
	pass->data = gnutls_malloc(pass->size);
	pass->data[0] = 0xDE;
	pass->data[1] = 0xAD;
	pass->data[2] = 0xBE;
	pass->data[3] = 0xEF;
	return 0;
}

static void tls_setup_peers(gnutls_session_t *client, gnutls_session_t *server,
			    const char *cprio, const char *sprio, int raw)
{
	gnutls_certificate_credentials_t clientx509cred;
	gnutls_certificate_credentials_t serverx509cred;
	gnutls_anon_client_credentials_t c_anoncred;
	gnutls_anon_server_credentials_t s_anoncred;
	gnutls_psk_client_credentials_t c_psk_cred;
	gnutls_psk_server_credentials_t s_psk_cred;
	const gnutls_datum_t pskkey = { (void *)"DEADBEEF", 8 };
	int cret = GNUTLS_E_AGAIN;
	int sret = GNUTLS_E_AGAIN;

	/* General init. */
	global_init();
	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(2);

	/* Init server */
	gnutls_certificate_allocate_credentials(&serverx509cred);
	if (raw)
		gnutls_certificate_set_rawpk_key_mem(
			serverx509cred, &rawpk_public_key1, &rawpk_private_key1,
			GNUTLS_X509_FMT_PEM, NULL, 0, NULL, 0, 0);
	else
		gnutls_certificate_set_x509_key_mem(serverx509cred,
						    &server_cert, &server_key,
						    GNUTLS_X509_FMT_PEM);
	gnutls_anon_allocate_server_credentials(&s_anoncred);
	gnutls_psk_allocate_server_credentials(&s_psk_cred);
	gnutls_psk_set_server_credentials_function(s_psk_cred, serv_psk_func);
	gnutls_init(server, GNUTLS_SERVER | GNUTLS_ENABLE_RAWPK);
	gnutls_credentials_set(*server, GNUTLS_CRD_CERTIFICATE, serverx509cred);
	gnutls_credentials_set(*server, GNUTLS_CRD_ANON, s_anoncred);
	gnutls_credentials_set(*server, GNUTLS_CRD_PSK, s_psk_cred);
	gnutls_priority_set_direct(*server, sprio, NULL);
	gnutls_transport_set_push_function(*server, server_push);
	gnutls_transport_set_pull_function(*server, server_pull);
	gnutls_transport_set_ptr(*server, *server);

	/* Init client */
	gnutls_certificate_allocate_credentials(&clientx509cred);
	gnutls_anon_allocate_client_credentials(&c_anoncred);
	gnutls_psk_allocate_client_credentials(&c_psk_cred);
	gnutls_psk_set_client_credentials(c_psk_cred, "psk", &pskkey,
					  GNUTLS_PSK_KEY_HEX);
	gnutls_init(client, GNUTLS_CLIENT | GNUTLS_ENABLE_RAWPK);
	gnutls_credentials_set(*client, GNUTLS_CRD_CERTIFICATE, clientx509cred);
	gnutls_credentials_set(*client, GNUTLS_CRD_ANON, c_anoncred);
	gnutls_credentials_set(*client, GNUTLS_CRD_PSK, c_psk_cred);
	gnutls_priority_set_direct(*client, cprio, NULL);
	gnutls_transport_set_push_function(*client, client_push);
	gnutls_transport_set_pull_function(*client, client_pull);
	gnutls_transport_set_ptr(*client, *client);

	HANDSHAKE(*client, *server);
}

static void tls_clear_peers(gnutls_session_t client, gnutls_session_t server)
{
	void *cred;
	gnutls_bye(client, GNUTLS_SHUT_RDWR);
	gnutls_bye(server, GNUTLS_SHUT_RDWR);

	if (gnutls_credentials_get(client, GNUTLS_CRD_CERTIFICATE, &cred) == 0)
		gnutls_certificate_free_credentials(cred);

	if (gnutls_credentials_get(server, GNUTLS_CRD_CERTIFICATE, &cred) == 0)
		gnutls_certificate_free_credentials(cred);

	if (gnutls_credentials_get(client, GNUTLS_CRD_ANON, &cred) == 0)
		gnutls_anon_free_client_credentials(cred);

	if (gnutls_credentials_get(server, GNUTLS_CRD_ANON, &cred) == 0)
		gnutls_anon_free_server_credentials(cred);

	if (gnutls_credentials_get(client, GNUTLS_CRD_PSK, &cred) == 0)
		gnutls_psk_free_client_credentials(cred);

	if (gnutls_credentials_get(server, GNUTLS_CRD_PSK, &cred) == 0)
		gnutls_psk_free_server_credentials(cred);

	gnutls_deinit(client);
	gnutls_deinit(server);

	gnutls_global_deinit();

	reset_buffers();
}

static void tlsv13_binding(void)
{
	gnutls_session_t client = NULL;
	gnutls_session_t server = NULL;
	unsigned char buffer[64];
	size_t transferred = 0;

	success("testing TLSv1.3 x509 channel binding\n");

	tls_setup_peers(&client, &server, "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.3",
			"NORMAL:+VERS-TLS1.3", 0);

	if (gnutls_protocol_get_version(client) != GNUTLS_TLS1_3)
		fail("TLS1.3 was not negotiated\n");

	TRANSFER(client, server, "xxxx", 4, buffer, sizeof(buffer));

	/* tls-unique testing - must fail under 1.3 */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_UNIQUE,
			       "tls-unique", 1) == 0)
		success("binding fail: tls-unique not supported for TLSv1.3\n");

	/* bogus biding type */
	if (check_binding_data(client, server, 666, "tls-fake", 1) == 0)
		success("binding fail: fake tls binding type not supported\n");

	/* tls-server-end-point testing, take both sides and compare */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_SERVER_END_POINT,
			       "tls-server-end-point", 0) == 0)
		success("binding match: tls-server-end-point\n");

	/* tls-exporter testing, take both sides and compare */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_EXPORTER,
			       "tls-exporter", 0) == 0)
		success("binding match: tls-exporter\n");

	tls_clear_peers(client, server);
}

static void rawv13_binding(void)
{
	gnutls_session_t client = NULL;
	gnutls_session_t server = NULL;
	unsigned char buffer[64];
	size_t transferred = 0;

	success("testing TLSv1.3 RAWPK channel binding\n");

	tls_setup_peers(
		&client, &server,
		"NORMAL:-VERS-TLS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-X25519:+CTYPE-ALL",
		"NORMAL:+VERS-TLS1.3:+ANON-ECDH:+ANON-DH:+ECDHE-RSA:+DHE-RSA:+RSA:+ECDHE-ECDSA:+CURVE-X25519:+SIGN-EDDSA-ED25519:+CTYPE-ALL",
		1);

	if (gnutls_protocol_get_version(client) != GNUTLS_TLS1_3)
		fail("TLS1.3 was not negotiated\n");

	TRANSFER(client, server, "xxxx", 4, buffer, sizeof(buffer));

	/* tls-unique testing - must fail under 1.3 */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_UNIQUE,
			       "tls-unique", 1) == 0)
		success("binding fail: tls-unique not supported for TLSv1.3\n");

	/* bogus biding type */
	if (check_binding_data(client, server, 666, "tls-fake", 1) == 0)
		success("binding fail: fake tls binding type not supported\n");

	/* tls-server-end-point testing, undefined for anon and psk */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_SERVER_END_POINT,
			       "tls-server-end-point", 1) == 0)
		success("binding fail: tls-server-end-point invalid for rawpk\n");

	/* tls-exporter testing, take both sides and compare */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_EXPORTER,
			       "tls-exporter", 0) == 0)
		success("binding match: tls-exporter\n");

	tls_clear_peers(client, server);
}

static void pskv13_binding(void)
{
	gnutls_session_t client = NULL;
	gnutls_session_t server = NULL;
	unsigned char buffer[64];
	size_t transferred = 0;

	success("testing TLSv1.3 PSK channel binding\n");

	tls_setup_peers(&client, &server,
			"NORMAL:-KX-ALL:+DHE-PSK:-VERS-TLS-ALL:+VERS-TLS1.3",
			"NORMAL:-KX-ALL:+DHE-PSK:+VERS-TLS1.3", 0);

	if (gnutls_protocol_get_version(client) != GNUTLS_TLS1_3)
		fail("TLS1.3 was not negotiated\n");

	TRANSFER(client, server, "xxxx", 4, buffer, sizeof(buffer));

	/* tls-unique testing - must fail under 1.3 */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_UNIQUE,
			       "tls-unique", 1) == 0)
		success("binding fail: tls-unique not supported for TLSv1.3\n");

	/* bogus biding type */
	if (check_binding_data(client, server, 666, "tls-fake", 1) == 0)
		success("binding fail: fake tls binding type not supported\n");

	/* tls-server-end-point testing, undefined for anon and psk */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_SERVER_END_POINT,
			       "tls-server-end-point", 1) == 0)
		success("binding fail: tls-server-end-point invalid for anon\n");

	/* tls-exporter testing, take both sides and compare */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_EXPORTER,
			       "tls-exporter", 0) == 0)
		success("binding match: tls-exporter\n");

	tls_clear_peers(client, server);
}

static void tlsv12_binding(void)
{
	gnutls_session_t client = NULL;
	gnutls_session_t server = NULL;
	unsigned char buffer[64];
	size_t transferred = 0;

	success("testing TLSv1.2 x509 channel binding\n");

	tls_setup_peers(&client, &server,
			"NORMAL:-VERS-TLS-ALL:+VERS-TLS1.1:+VERS-TLS1.2",
			"NORMAL:-VERS-TLS-ALL:+VERS-TLS1.1:+VERS-TLS1.2", 0);

	if (gnutls_protocol_get_version(client) != GNUTLS_TLS1_2)
		fail("TLS1.2 was not negotiated\n");

	TRANSFER(client, server, "xxxx", 4, buffer, sizeof(buffer));

	/* tls-unique testing - must succeed under 1.2 */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_UNIQUE,
			       "tls-unique", 0) == 0)
		success("binding match: tls-unique\n");

	/* bogus biding type */
	if (check_binding_data(client, server, 666, "tls-fake", 1) == 0)
		success("binding fail: fake binding type not supported\n");

	/* tls-server-end-point testing, take both sides and compare */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_SERVER_END_POINT,
			       "tls-server-end-point", 0) == 0)
		success("binding match: tls-server-end-point\n");

	/* tls-exporter testing, take both sides and compare */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_EXPORTER,
			       "tls-exporter", 0) == 0)
		success("binding match: tls-exporter\n");

	tls_clear_peers(client, server);
}

static void anon12_binding(void)
{
	gnutls_session_t client = NULL;
	gnutls_session_t server = NULL;
	unsigned char buffer[64];
	size_t transferred = 0;

	success("testing TLSv1.2 ANON channel binding\n");

	tls_setup_peers(&client, &server,
			"NORMAL:-KX-ALL:+ANON-DH:-VERS-ALL:+VERS-TLS1.2",
			"NORMAL:-KX-ALL:+ANON-DH:-VERS-ALL:+VERS-TLS1.2", 0);

	if (gnutls_protocol_get_version(client) != GNUTLS_TLS1_2)
		fail("TLS1.2 was not negotiated\n");

	TRANSFER(client, server, "xxxx", 4, buffer, sizeof(buffer));

	/* tls-unique testing - must succeed under 1.2 */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_UNIQUE,
			       "tls-unique", 0) == 0)
		success("binding match: tls-unique\n");

	/* bogus biding type */
	if (check_binding_data(client, server, 666, "tls-fake", 1) == 0)
		success("binding fail: fake binding type not supported\n");

	/* tls-server-end-point testing, undefined for anon and psk */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_SERVER_END_POINT,
			       "tls-server-end-point", 1) == 0)
		success("binding fail: tls-server-end-point invalid for anon\n");

	/* tls-exporter testing, take both sides and compare */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_EXPORTER,
			       "tls-exporter", 0) == 0)
		success("binding match: tls-exporter\n");

	tls_clear_peers(client, server);
}

static void pskv12_binding(void)
{
	gnutls_session_t client = NULL;
	gnutls_session_t server = NULL;
	unsigned char buffer[64];
	size_t transferred = 0;

	success("testing TLSv1.2 PSK channel binding\n");

	tls_setup_peers(&client, &server,
			"NORMAL:-KX-ALL:+DHE-PSK:-VERS-ALL:+VERS-TLS1.2",
			"NORMAL:-KX-ALL:+DHE-PSK:-VERS-ALL:+VERS-TLS1.2", 0);

	if (gnutls_protocol_get_version(client) != GNUTLS_TLS1_2)
		fail("TLS1.2 was not negotiated\n");

	TRANSFER(client, server, "xxxx", 4, buffer, sizeof(buffer));

	/* tls-unique testing - must succeed under 1.2 */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_UNIQUE,
			       "tls-unique", 0) == 0)
		success("binding match: tls-unique\n");

	/* bogus biding type */
	if (check_binding_data(client, server, 666, "tls-fake", 1) == 0)
		success("binding fail: fake binding type not supported\n");

	/* tls-server-end-point testing, undefined for anon and psk */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_SERVER_END_POINT,
			       "tls-server-end-point", 1) == 0)
		success("binding fail: tls-server-end-point invalid for anon\n");

	/* tls-exporter testing, take both sides and compare */
	if (check_binding_data(client, server, GNUTLS_CB_TLS_EXPORTER,
			       "tls-exporter", 0) == 0)
		success("binding match: tls-exporter\n");

	tls_clear_peers(client, server);
}

void doit(void)
{
	tlsv13_binding();
	tlsv12_binding();
	rawv13_binding();
	anon12_binding();
	pskv13_binding();
	pskv12_binding();
}
