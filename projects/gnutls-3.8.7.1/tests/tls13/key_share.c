/*
 * Copyright (C) 2017-2018 Red Hat, Inc.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gnutls/gnutls.h>

#include "cert-common.h"
#include "utils.h"
#include "tls13/ext-parse.h"
#include "eagain-common.h"

/* This program tests the Key Share behavior in Client Hello,
 * and whether the flags to gnutls_init for key share are followed.
 */

const char *testname = "";

#define myfail(fmt, ...) fail("%s: " fmt, testname, ##__VA_ARGS__)

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

unsigned int tls_id_to_group[] = {
	[23] = GNUTLS_GROUP_SECP256R1,	  [24] = GNUTLS_GROUP_SECP384R1,
	[29] = GNUTLS_GROUP_X25519,	  [30] = GNUTLS_GROUP_X448,
	[0x100] = GNUTLS_GROUP_FFDHE2048, [0x101] = GNUTLS_GROUP_FFDHE3072
};

#define TLS_EXT_KEY_SHARE 51

typedef struct ctx_st {
	gnutls_group_t group;
	unsigned ngroups;
} ctx_st;

static void check_ks_contents(void *priv, gnutls_datum_t *msg)
{
	ctx_st *ctx;
	int len;
	gnutls_session_t session = priv;
	int pos;
	unsigned total = 0, id;
	unsigned found = 0;

	ctx = gnutls_session_get_ptr(session);

	len = (msg->data[0] << 8) | msg->data[1];
	if (len + 2 != (int)msg->size)
		myfail("mismatch in length (%d vs %d)!\n", len, (int)msg->size);

	pos = 2;

	while ((unsigned)pos < msg->size) {
		id = (msg->data[pos] << 8) | msg->data[pos + 1];
		pos += 2;
		len -= 2;

		if (debug)
			success("found group: %u\n", id);
		if (id < sizeof(tls_id_to_group) / sizeof(tls_id_to_group[0])) {
			if (tls_id_to_group[id] == ctx->group)
				found = 1;
		}
		total++;

		SKIP16(pos, msg->size);
	}

	if (total != ctx->ngroups) {
		myfail("found %d groups, expected %d\n", total, ctx->ngroups);
	}

	if (found == 0) {
		myfail("did not find group %s\n",
		       gnutls_group_get_name(ctx->group));
	}
}

static int client_hello_callback(gnutls_session_t session, unsigned int htype,
				 unsigned post, unsigned int incoming,
				 const gnutls_datum_t *msg)
{
	if (htype == GNUTLS_HANDSHAKE_CLIENT_HELLO &&
	    post == GNUTLS_HOOK_POST) {
		if (find_client_extension(msg, TLS_EXT_KEY_SHARE, session,
					  check_ks_contents) == 0)
			fail("Could not find key share extension!\n");
	}

	return 0;
}

static void start(const char *name, const char *prio, unsigned flag,
		  gnutls_group_t group, unsigned ngroups)
{
	int sret, cret;
	gnutls_certificate_credentials_t scred, ccred;
	gnutls_session_t server, client;
	ctx_st ctx;

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

	gnutls_handshake_set_hook_function(server, GNUTLS_HANDSHAKE_ANY,
					   GNUTLS_HOOK_BOTH,
					   client_hello_callback);
	ctx.group = group;
	ctx.ngroups = ngroups;
	gnutls_session_set_ptr(server, &ctx);

	/* avoid calling all the priority functions, since the defaults
	 * are adequate.
	 */
	gnutls_priority_set_direct(server, "NORMAL:+VERS-TLS1.3", NULL);

	gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE, scred);
	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);

	/* Init client */
	gnutls_certificate_allocate_credentials(&ccred);
	assert(gnutls_certificate_set_x509_trust_mem(ccred, &ca3_cert,
						     GNUTLS_X509_FMT_PEM) >= 0);

	gnutls_init(&client, GNUTLS_CLIENT | flag);

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

	if (gnutls_group_get(server) != group)
		myfail("group doesn't match the expected: %s\n",
		       gnutls_group_get_name(gnutls_group_get(server)));

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
	start("single group: default secp256r1",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3", GNUTLS_KEY_SHARE_TOP,
	      GNUTLS_GROUP_SECP256R1, 1);
	start("single group: secp256r1",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-X25519:+GROUP-FFDHE2048",
	      GNUTLS_KEY_SHARE_TOP, GNUTLS_GROUP_SECP256R1, 1);
	start("single group: x25519",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-X25519:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-FFDHE2048",
	      GNUTLS_KEY_SHARE_TOP, GNUTLS_GROUP_X25519, 1);

	/* unfortunately we strictly follow the rfc7919 RFC and we prioritize groups
	 * based on ciphersuite listing as well. To prioritize the FFDHE groups we need
	 * to prioritize the non-EC ciphersuites first. */
	start("single group: ffdhe2048",
	      "NORMAL:-KX-ALL:+DHE-RSA:+ECDHE-RSA:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-FFDHE2048:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-X25519:+GROUP-FFDHE3072",
	      GNUTLS_KEY_SHARE_TOP, GNUTLS_GROUP_FFDHE2048, 1);

	start("two groups: default secp256r1", "NORMAL:-VERS-ALL:+VERS-TLS1.3",
	      GNUTLS_KEY_SHARE_TOP2, GNUTLS_GROUP_SECP256R1, 2);
	start("two groups: secp256r1",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-X25519:+GROUP-FFDHE2048",
	      GNUTLS_KEY_SHARE_TOP2, GNUTLS_GROUP_SECP256R1, 2);
	start("two groups: x25519",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-X25519:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-FFDHE2048",
	      GNUTLS_KEY_SHARE_TOP2, GNUTLS_GROUP_X25519, 2);
	start("two groups: x448",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-X448:+GROUP-X25519:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-FFDHE2048",
	      GNUTLS_KEY_SHARE_TOP2, GNUTLS_GROUP_X448, 2);
	start("two groups: ffdhe2048",
	      "NORMAL:-KX-ALL:+DHE-RSA:+ECDHE-RSA:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-FFDHE2048:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-X25519:+GROUP-FFDHE3072",
	      GNUTLS_KEY_SHARE_TOP2, GNUTLS_GROUP_FFDHE2048, 2);

	start("three groups: default secp256r1",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3", GNUTLS_KEY_SHARE_TOP3,
	      GNUTLS_GROUP_SECP256R1, 3);
	start("three groups: secp256r1",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-X25519:+GROUP-FFDHE2048",
	      GNUTLS_KEY_SHARE_TOP3, GNUTLS_GROUP_SECP256R1, 3);
	start("three groups: x25519",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-X25519:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-FFDHE2048",
	      GNUTLS_KEY_SHARE_TOP3, GNUTLS_GROUP_X25519, 3);
	start("three groups: x448",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-X448:+GROUP-X25519:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-FFDHE2048",
	      GNUTLS_KEY_SHARE_TOP3, GNUTLS_GROUP_X448, 3);
	start("three groups: ffdhe2048",
	      "NORMAL:-KX-ALL:+DHE-RSA:+ECDHE-RSA:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-FFDHE2048:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-X25519:+GROUP-FFDHE3072",
	      GNUTLS_KEY_SHARE_TOP3, GNUTLS_GROUP_FFDHE2048, 3);

	/* test default behavior */
	start("default groups(2): default secp256r1",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3", 0, GNUTLS_GROUP_SECP256R1, 2);
	start("default groups(2): secp256r1",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-X25519:+GROUP-FFDHE2048",
	      0, GNUTLS_GROUP_SECP256R1, 2);
	start("default groups(2): x25519",
	      "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-X25519:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-FFDHE2048",
	      0, GNUTLS_GROUP_X25519, 2);
	start("default groups(2): ffdhe2048",
	      "NORMAL:-KX-ALL:+DHE-RSA:+ECDHE-RSA:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-FFDHE2048:+GROUP-SECP256R1:+GROUP-SECP384R1:+GROUP-X25519:+GROUP-FFDHE3072",
	      0, GNUTLS_GROUP_FFDHE2048, 2);
}
