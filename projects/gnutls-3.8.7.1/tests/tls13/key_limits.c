/*
 * Copyright (C) 2012 Free Software Foundation, Inc.
 * Copyright (C) 2017 Red Hat, Inc.
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

#include <gnutls/gnutls.h>
#include <assert.h>

#include "utils.h"
#include "cert-common.h"
#include "eagain-common.h"

/* This program tests whether re-key occurs at the expected
 * time.
 */

const char *testname = "";

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

#define MAX_BUF 1024

static void start(const char *name, const char *prio, unsigned exp_update)
{
	int sret, cret;
	gnutls_certificate_credentials_t scred, ccred;
	gnutls_session_t server, client;

	char buffer[MAX_BUF + 1];
	unsigned char seq[8];
	unsigned update_happened = 0;
	unsigned i;

	testname = name;
	success("== test %s ==\n", testname);

	global_init();
	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(9);

	/* Init Server */
	assert(gnutls_certificate_allocate_credentials(&scred) >= 0);
	assert(gnutls_certificate_set_x509_key_mem(scred, &server_cert,
						   &server_key,
						   GNUTLS_X509_FMT_PEM) >= 0);

	gnutls_init(&server, GNUTLS_SERVER);

	/* avoid calling all the priority functions, since the defaults
	 * are adequate.
	 */
	sret = gnutls_priority_set_direct(server, prio, NULL);
	if (sret < 0) {
		fail("error in priority '%s': %s\n", prio,
		     gnutls_strerror(sret));
	}

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
		fail("cannot set TLS 1.3 priorities\n");

	gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE, ccred);

	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);

	/* Perform handshake */
	HANDSHAKE(client, server);
	if (debug)
		success("Handshake established\n");

	assert(gnutls_record_get_state(server, 0, NULL, NULL, NULL, seq) >= 0);
	assert(gnutls_record_set_state(
		       server, 0, (void *)"\x00\x00\x00\x00\x00\xff\xff\xfa") >=
	       0);

	assert(gnutls_record_get_state(client, 1, NULL, NULL, NULL, seq) >= 0);
	assert(gnutls_record_set_state(
		       client, 1, (void *)"\x00\x00\x00\x00\x00\xff\xff\xfa") >=
	       0);

	memset(buffer, 1, sizeof(buffer));

	for (i = 0; i < 32; i++) {
		usleep(10000); /* some systems like FreeBSD have their buffers full during this send */
		do {
			sret = gnutls_record_send(server, buffer,
						  sizeof(buffer));
		} while (sret == GNUTLS_E_AGAIN ||
			 sret == GNUTLS_E_INTERRUPTED);

		if (sret < 0) {
			fail("Error sending %d byte packet: %s\n",
			     (int)sizeof(buffer), gnutls_strerror(sret));
		}

		if (sret != sizeof(buffer)) {
			fail("Error sending %d byte packet: sent: %d\n",
			     (int)sizeof(buffer), sret);
		}
		do {
			cret = gnutls_record_recv_seq(client, buffer, MAX_BUF,
						      seq);
		} while (cret == GNUTLS_E_AGAIN ||
			 cret == GNUTLS_E_INTERRUPTED);

		if (memcmp(seq, "\x00\x00\x00\x00\x00\x00\x00\x01", 8) == 0) {
			update_happened = 1;
		}
	}

	gnutls_bye(client, GNUTLS_SHUT_WR);
	gnutls_bye(server, GNUTLS_SHUT_WR);

	gnutls_deinit(client);
	gnutls_deinit(server);

	gnutls_certificate_free_credentials(scred);
	gnutls_certificate_free_credentials(ccred);

	gnutls_global_deinit();
	reset_buffers();

	if (exp_update && update_happened == 0) {
		fail("no update occurred!\n");
	} else if (!exp_update && update_happened) {
		fail("update occurred unexpectedly!\n");
	} else {
		if (debug)
			success("detected update!\n");
	}
}

#define AES_GCM "NORMAL:-VERS-ALL:+VERS-TLS1.3:-CIPHER-ALL:+AES-128-GCM"
#define CHACHA_POLY1305 \
	"NORMAL:-VERS-ALL:+VERS-TLS1.3:-CIPHER-ALL:+CHACHA20-POLY1305"

void doit(void)
{
	start("aes-gcm", AES_GCM, 1);
	if (!gnutls_fips140_mode_enabled()) {
		start("chacha20", CHACHA_POLY1305, 0);
	}
}
