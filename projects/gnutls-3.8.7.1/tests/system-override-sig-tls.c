/*
 * Copyright (C) 2015-2021 Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gnutls/gnutls.h>
#include "utils.h"

#define SKIP16(pos, total)                                       \
	{                                                        \
		uint16_t _s;                                     \
		if (pos + 2 > total)                             \
			fail("error\n");                         \
		_s = (msg->data[pos] << 8) | msg->data[pos + 1]; \
		if ((size_t)(pos + 2 + _s) > total)              \
			fail("error\n");                         \
		pos += 2 + _s;                                   \
	}

#define SKIP8(pos, total)                           \
	{                                           \
		uint8_t _s;                         \
		if (pos + 1 > total)                \
			fail("error\n");            \
		_s = msg->data[pos];                \
		if ((size_t)(pos + 1 + _s) > total) \
			fail("error\n");            \
		pos += 1 + _s;                      \
	}

#define HANDSHAKE_SESSION_ID_POS 34

#include "eagain-common.h"
#include "cert-common.h"

/* This tests whether the client omits signature algorithms marked as insecure,
 * from the signature_algorithms extension.
 */

const char *side;

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

#define PRIO                                       \
	"NORMAL:-VERS-ALL:+VERS-TLS1.3:-SIGN-ALL:" \
	"+SIGN-RSA-PSS-RSAE-SHA256:+SIGN-RSA-PSS-RSAE-SHA384"
/* rsa_pss_rsae_sha384 */
#define SIGALGS_EXP "\x00\x02\x08\x05"

static int ext_callback(void *ctx, unsigned tls_id, const unsigned char *data,
			unsigned size)
{
	if (tls_id == 13) { /* signature algorithms */
		if (size != sizeof(SIGALGS_EXP) - 1) {
			fail("invalid signature_algorithms length: %u != 4\n",
			     size);
		}
		if (memcmp(data, SIGALGS_EXP, sizeof(SIGALGS_EXP) - 1) != 0) {
			fail("invalid signature_algorithms\n");
		}
	}
	return 0;
}

static int handshake_callback(gnutls_session_t session, unsigned int htype,
			      unsigned post, unsigned int incoming,
			      const gnutls_datum_t *msg)
{
	assert(post);

	if (!incoming && htype == GNUTLS_HANDSHAKE_CLIENT_HELLO) {
		int ret;
		unsigned pos;
		gnutls_datum_t mmsg;

		assert(msg->size >= HANDSHAKE_SESSION_ID_POS);
		pos = HANDSHAKE_SESSION_ID_POS;
		SKIP8(pos, msg->size);
		SKIP16(pos, msg->size);
		SKIP8(pos, msg->size);

		mmsg.data = &msg->data[pos];
		mmsg.size = msg->size - pos;
		ret = gnutls_ext_raw_parse(NULL, ext_callback, &mmsg, 0);
		assert(ret >= 0);
	}
	return 0;
}

void doit(void)
{
	int ret;
	/* Server stuff. */
	gnutls_certificate_credentials_t serverx509cred;
	gnutls_session_t server;
	int sret = GNUTLS_E_AGAIN;
	/* Client stuff. */
	gnutls_certificate_credentials_t clientx509cred;
	gnutls_session_t client;
	int cret = GNUTLS_E_AGAIN;

	global_init();

	/* General init. */
	gnutls_global_set_log_function(tls_log_func);
	if (debug)
		gnutls_global_set_log_level(6);

	/* Init server */
	gnutls_certificate_allocate_credentials(&serverx509cred);
	gnutls_certificate_set_x509_key_mem(serverx509cred, &server2_cert,
					    &server2_key, GNUTLS_X509_FMT_PEM);

	gnutls_init(&server, GNUTLS_SERVER);
	gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE, serverx509cred);

	gnutls_priority_set_direct(server, PRIO, NULL);

	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_pull_timeout_function(server,
						   server_pull_timeout_func);
	gnutls_transport_set_ptr(server, server);

	/* Init client */
	ret = gnutls_certificate_allocate_credentials(&clientx509cred);
	if (ret < 0)
		exit(1);

	ret = gnutls_certificate_set_x509_trust_mem(clientx509cred, &ca2_cert,
						    GNUTLS_X509_FMT_PEM);
	if (ret < 0)
		exit(1);

	ret = gnutls_init(&client, GNUTLS_CLIENT);
	if (ret < 0)
		exit(1);

	ret = gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE,
				     clientx509cred);
	if (ret < 0)
		exit(1);

	ret = gnutls_priority_set_direct(client, PRIO, NULL);
	if (ret < 0)
		exit(1);

	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_pull_timeout_function(client,
						   client_pull_timeout_func);
	gnutls_transport_set_ptr(client, client);

	gnutls_handshake_set_hook_function(client, GNUTLS_HANDSHAKE_ANY,
					   GNUTLS_HOOK_POST,
					   handshake_callback);

	HANDSHAKE(client, server);

	gnutls_bye(client, GNUTLS_SHUT_RDWR);
	gnutls_bye(server, GNUTLS_SHUT_RDWR);

	gnutls_deinit(client);
	gnutls_deinit(server);

	gnutls_certificate_free_credentials(serverx509cred);
	gnutls_certificate_free_credentials(clientx509cred);

	gnutls_global_deinit();

	reset_buffers();
}
