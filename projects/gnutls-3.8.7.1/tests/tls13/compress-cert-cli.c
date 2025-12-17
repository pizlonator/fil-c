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

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || !defined(HAVE_LIBZ) || !defined(HAVE_LIBBROTLI) || \
	!defined(HAVE_LIBZSTD)

int main(int argc, char **argv)
{
	exit(77);
}

#else

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <assert.h>
#include "cert-common.h"

#include "utils.h"
#include "eagain-common.h"

const char *side = "";

static void tls_log_func(int level, const char *str)
{
	fprintf(stderr, "%s|<%d>| %s", side, level, str);
}

struct handshake_cb_data_st {
	bool is_server;
	bool found_compress_certificate;
	bool found_compressed_certificate;
	bool found_certificate;
};

static int ext_callback(void *ctx, unsigned tls_id, const unsigned char *data,
			unsigned size)
{
	struct handshake_cb_data_st *cb_data = ctx;
	if (tls_id == 27) { /* compress_certificate */
		cb_data->found_compress_certificate = 1;
	}
	return 0;
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

static int handshake_callback(gnutls_session_t session, unsigned int htype,
			      unsigned post, unsigned int incoming,
			      const gnutls_datum_t *msg)
{
	struct handshake_cb_data_st *data = gnutls_session_get_ptr(session);
	unsigned pos = 0;
	gnutls_datum_t mmsg;
	int ret;

	if ((data->is_server && incoming) || (!data->is_server && !incoming)) {
		return 0;
	}

	switch (htype) {
	case GNUTLS_HANDSHAKE_CERTIFICATE_REQUEST:
		SKIP8(pos, msg->size);

		mmsg.data = &msg->data[pos];
		mmsg.size = msg->size - pos;
		ret = gnutls_ext_raw_parse(data, ext_callback, &mmsg, 0);
		assert(ret >= 0);
		break;
	case GNUTLS_HANDSHAKE_COMPRESSED_CERTIFICATE_PKT:
		data->found_compressed_certificate = true;
		break;
	case GNUTLS_HANDSHAKE_CERTIFICATE_PKT:
		data->found_certificate = true;
		break;
	default:
		break;
	}

	return 0;
}

static void run(void)
{
	/* Server stuff. */
	gnutls_certificate_credentials_t scred;
	gnutls_session_t server;
	gnutls_compression_method_t smethods[] = { GNUTLS_COMP_ZSTD,
						   GNUTLS_COMP_BROTLI,
						   GNUTLS_COMP_ZLIB };
	struct handshake_cb_data_st sdata = { 0, false, false, false };
	int sret;
	/* Client stuff. */
	gnutls_certificate_credentials_t ccred;
	gnutls_session_t client;
	gnutls_compression_method_t cmethods[] = { GNUTLS_COMP_ZLIB,
						   GNUTLS_COMP_BROTLI };
	struct handshake_cb_data_st cdata = { 0, false, false, false };
	int cret;
	/* Need to enable anonymous KX specifically. */
	int ret;

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
	ret = gnutls_priority_set_direct(
		server, "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.3", NULL);
	if (ret < 0)
		exit(1);

	ret = gnutls_compress_certificate_set_methods(
		server, smethods, sizeof(smethods) / sizeof(*smethods));
	if (ret < 0) {
		fail("server: setting compression method failed (%s)\n",
		     gnutls_strerror(ret));
	}
	sdata.is_server = true;
	gnutls_session_set_ptr(server, &sdata);
	gnutls_handshake_set_hook_function(server, GNUTLS_HANDSHAKE_ANY,
					   GNUTLS_HOOK_POST,
					   handshake_callback);

	gnutls_credentials_set(server, GNUTLS_CRD_CERTIFICATE, scred);
	gnutls_transport_set_push_function(server, server_push);
	gnutls_transport_set_pull_function(server, server_pull);
	gnutls_transport_set_ptr(server, server);

	/* Init client */
	assert(gnutls_certificate_allocate_credentials(&ccred) >= 0);
	assert(gnutls_certificate_set_x509_key_mem(ccred, &cli_ca3_cert_chain,
						   &cli_ca3_key,
						   GNUTLS_X509_FMT_PEM) >= 0);
	assert(gnutls_certificate_set_x509_trust_mem(ccred, &ca3_cert,
						     GNUTLS_X509_FMT_PEM) >= 0);

	gnutls_init(&client, GNUTLS_CLIENT);
	ret = gnutls_priority_set_direct(
		client, "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.3", NULL);
	assert(ret >= 0);

	ret = gnutls_credentials_set(client, GNUTLS_CRD_CERTIFICATE, ccred);
	if (ret < 0)
		exit(1);

	ret = gnutls_compress_certificate_set_methods(
		client, cmethods, sizeof(cmethods) / sizeof(*cmethods));
	if (ret < 0) {
		fail("client: setting compression method failed (%s)\n",
		     gnutls_strerror(ret));
	}
	cdata.is_server = false;
	gnutls_session_set_ptr(client, &cdata);
	gnutls_handshake_set_hook_function(client, GNUTLS_HANDSHAKE_ANY,
					   GNUTLS_HOOK_POST,
					   handshake_callback);

	gnutls_transport_set_push_function(client, client_push);
	gnutls_transport_set_pull_function(client, client_pull);
	gnutls_transport_set_ptr(client, client);

	HANDSHAKE(client, server);
	if (debug)
		success("Handshake established\n");

	if (!sdata.found_compress_certificate) {
		fail("server: compress_certificate extension not sent\n");
	}
	if (!sdata.found_compressed_certificate) {
		fail("server: CompressedCertificate not sent\n");
	}
	if (sdata.found_certificate) {
		fail("server: Certificate sent\n");
	}
	if (!cdata.found_compress_certificate) {
		fail("client: compress_certificate extension not received\n");
	}
	if (!cdata.found_compressed_certificate) {
		fail("client: CompressedCertificate not received\n");
	}
	if (cdata.found_certificate) {
		fail("client: Certificate not received\n");
	}

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
	run();
}

#endif /* _WIN32 */
