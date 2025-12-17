/*
 * Copyright (C) 2023 Red Hat, Inc.
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

#include "utils.h"
#include "cert-common.h"
#include "eagain-common.h"

/* This program tests whether forced extended master secret is
 * negotiated as expected.
 */

const char *side;

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

static void try(const char *name, const char *sprio, const char *cprio,
		int serr, int cerr)
{
	int sret, cret;
	gnutls_certificate_credentials_t scred, ccred;
	gnutls_session_t server, client;

	success("Running %s\n", name);

	assert(gnutls_certificate_allocate_credentials(&scred) >= 0);

	assert(gnutls_certificate_set_x509_key_mem(
		       scred, &server_ca3_localhost_cert, &server_ca3_key,
		       GNUTLS_X509_FMT_PEM) >= 0);

	assert(gnutls_certificate_allocate_credentials(&ccred) >= 0);

	assert(gnutls_certificate_set_x509_trust_mem(ccred, &ca3_cert,
						     GNUTLS_X509_FMT_PEM) >= 0);

	assert(gnutls_init(&server, GNUTLS_SERVER) >= 0);
	assert(gnutls_init(&client, GNUTLS_CLIENT) >= 0);

	gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE, scred);
	gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE, ccred);

	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);
	assert(gnutls_priority_set_direct(server, sprio, 0) >= 0);

	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);
	assert(gnutls_priority_set_direct(client, cprio, 0) >= 0);

	HANDSHAKE_EXPECT(client, server, cerr, serr);

	gnutls_deinit(server);
	gnutls_deinit(client);
	gnutls_certificate_free_credentials(scred);
	gnutls_certificate_free_credentials(ccred);

	reset_buffers();
}

#define AES_GCM "NORMAL:-VERS-ALL:+VERS-TLS1.2"

void doit(void)
{
	gnutls_fips140_context_t fips_context;

	global_init();

	/* General init. */
	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(2);

	assert(gnutls_fips140_context_init(&fips_context) >= 0);

	/* Default: EMS is requested in non-FIPS mode, while it is
	 * required in FIPS mode.
	 */
	FIPS_PUSH_CONTEXT();
	try("default", AES_GCM, AES_GCM, 0, 0);
	FIPS_POP_CONTEXT(APPROVED);

	FIPS_PUSH_CONTEXT();
	try("both force EMS", AES_GCM ":%FORCE_SESSION_HASH",
	    AES_GCM ":%FORCE_SESSION_HASH", 0, 0);
	FIPS_POP_CONTEXT(APPROVED);

	if (gnutls_fips140_mode_enabled()) {
		try("neither negotiates EMS", AES_GCM ":%NO_SESSION_HASH",
		    AES_GCM ":%NO_SESSION_HASH", GNUTLS_E_INSUFFICIENT_SECURITY,
		    GNUTLS_E_AGAIN);
	} else {
		try("neither negotiates EMS", AES_GCM ":%NO_SESSION_HASH",
		    AES_GCM ":%NO_SESSION_HASH", 0, 0);
	}
	/* Note that the error codes are swapped based on FIPS mode:
	 * in FIPS mode, the server doesn't send the extension which
	 * causes the client to not send the one either, and then the
	 * server doesn't like the situation.  On the other hand, in
	 * non-FIPS mode, it's the client to decide to abort the
	 * connection.
	 */
	if (gnutls_fips140_mode_enabled()) {
		try("server doesn't negotiate EMS, client forces EMS",
		    AES_GCM ":%NO_SESSION_HASH", AES_GCM ":%FORCE_SESSION_HASH",
		    GNUTLS_E_INSUFFICIENT_SECURITY, GNUTLS_E_AGAIN);
	} else {
		try("server doesn't negotiate EMS, client forces EMS",
		    AES_GCM ":%NO_SESSION_HASH", AES_GCM ":%FORCE_SESSION_HASH",
		    GNUTLS_E_AGAIN, GNUTLS_E_INSUFFICIENT_SECURITY);
	}
	try("server forces EMS, client doesn't negotiate EMS",
	    AES_GCM ":%FORCE_SESSION_HASH", AES_GCM ":%NO_SESSION_HASH",
	    GNUTLS_E_INSUFFICIENT_SECURITY, GNUTLS_E_AGAIN);

	gnutls_fips140_context_deinit(fips_context);

	gnutls_global_deinit();
}
