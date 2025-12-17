/*
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * Author: Zoltan Fridrich
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

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <gnutls/gnutls.h>

#include "cert-common.h"
#include "utils.h"

/* This program tests whether the compress_certificate extensions is disabled
 * when client and server have incompatible compression methods set */

#define PRIO "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.3"
#define CHECK(X) assert((X) >= 0)

static pid_t child;
int client_bad;
int server_bad;

static void terminate(void)
{
	int status = 0;

	if (child) {
		kill(child, SIGTERM);
		wait(&status);
	}
	exit(1);
}

static void client_log_func(int level, const char *str)
{
	fprintf(stderr, "client|<%d>| %s", level, str);
}

static void server_log_func(int level, const char *str)
{
	fprintf(stderr, "server|<%d>| %s", level, str);
}

static int client_callback(gnutls_session_t session, unsigned htype,
			   unsigned post, unsigned incoming,
			   const gnutls_datum_t *msg)
{
	client_bad = 1;
	return 0;
}

static int server_callback(gnutls_session_t session, unsigned htype,
			   unsigned post, unsigned incoming,
			   const gnutls_datum_t *msg)
{
	server_bad = 1;
	return 0;
}

static void client(int fd)
{
	int ret;
	unsigned status;
	gnutls_session_t session;
	gnutls_certificate_credentials_t x509_cred;
	gnutls_compression_method_t method;
	gnutls_compression_method_t methods[] = { GNUTLS_COMP_BROTLI,
						  GNUTLS_COMP_ZSTD };
	size_t methods_len =
		sizeof(methods) / sizeof(gnutls_compression_method_t);

	global_init();

	if (debug) {
		gnutls_global_set_log_function(client_log_func);
		gnutls_global_set_log_level(4711);
	}

	CHECK(gnutls_certificate_allocate_credentials(&x509_cred));
	CHECK(gnutls_certificate_set_x509_trust_mem(x509_cred, &ca3_cert,
						    GNUTLS_X509_FMT_PEM));
	CHECK(gnutls_certificate_set_x509_key_mem(
		x509_cred, &cli_ca3_cert_chain, &cli_ca3_key,
		GNUTLS_X509_FMT_PEM));
	CHECK(gnutls_init(&session, GNUTLS_CLIENT));
	CHECK(gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
				     x509_cred));
	CHECK(gnutls_priority_set_direct(session, PRIO, NULL));

	ret = gnutls_compress_certificate_set_methods(session, methods,
						      methods_len);
	if (ret < 0) {
		fail("client: setting compression method failed (%s)\n\n",
		     gnutls_strerror(ret));
		terminate();
	}

	gnutls_handshake_set_hook_function(
		session, GNUTLS_HANDSHAKE_COMPRESSED_CERTIFICATE_PKT,
		GNUTLS_HOOK_PRE, client_callback);
	gnutls_transport_set_int(session, fd);

	do {
		ret = gnutls_handshake(session);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
	if (ret < 0) {
		fail("client: Handshake failed: %s\n", strerror(ret));
		goto cleanup;
	}
	if (debug)
		success("client: Handshake was completed\n");
	if (debug)
		success("client: TLS version is: %s\n",
			gnutls_protocol_get_name(
				gnutls_protocol_get_version(session)));

	method = gnutls_compress_certificate_get_selected_method(session);
	if (method != GNUTLS_COMP_UNKNOWN)
		fail("client: compression method should should not be set\n");

	if (client_bad)
		fail("client: certificate should not be compressed\n");

	ret = gnutls_certificate_verify_peers2(session, &status);
	if (ret < 0)
		fail("client: could not verify server certificate: %s\n",
		     gnutls_strerror(ret));
	if (status)
		fail("client: certificate verification failed\n");

	gnutls_bye(session, GNUTLS_SHUT_WR);

	if (debug)
		success("client: finished\n");

cleanup:
	close(fd);
	gnutls_deinit(session);
	gnutls_certificate_free_credentials(x509_cred);
	gnutls_global_deinit();
}

static void server(int fd)
{
	int ret;
	unsigned status;
	gnutls_session_t session;
	gnutls_certificate_credentials_t x509_cred;
	gnutls_compression_method_t method;
	gnutls_compression_method_t methods[] = { GNUTLS_COMP_ZLIB };
	size_t methods_len =
		sizeof(methods) / sizeof(gnutls_compression_method_t);

	global_init();

	if (debug) {
		gnutls_global_set_log_function(server_log_func);
		gnutls_global_set_log_level(4711);
	}

	CHECK(gnutls_certificate_allocate_credentials(&x509_cred));
	CHECK(gnutls_certificate_set_x509_trust_mem(x509_cred, &ca3_cert,
						    GNUTLS_X509_FMT_PEM));
	CHECK(gnutls_certificate_set_x509_key_mem(
		x509_cred, &server_ca3_localhost_cert_chain, &server_ca3_key,
		GNUTLS_X509_FMT_PEM));
	CHECK(gnutls_init(&session, GNUTLS_SERVER));
	CHECK(gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
				     x509_cred));
	CHECK(gnutls_priority_set_direct(session, PRIO, NULL));

	ret = gnutls_compress_certificate_set_methods(session, methods,
						      methods_len);
	if (ret < 0) {
		fail("server: setting compression method failed (%s)\n\n",
		     gnutls_strerror(ret));
		terminate();
	}

	gnutls_handshake_set_hook_function(
		session, GNUTLS_HANDSHAKE_COMPRESSED_CERTIFICATE_PKT,
		GNUTLS_HOOK_PRE, server_callback);
	gnutls_certificate_server_set_request(session, GNUTLS_CERT_REQUEST);
	gnutls_transport_set_int(session, fd);

	do {
		ret = gnutls_handshake(session);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
	if (ret < 0) {
		fail("server: Handshake has failed (%s)\n\n",
		     gnutls_strerror(ret));
		goto cleanup;
	}
	if (debug)
		success("server: Handshake was completed\n");
	if (debug)
		success("server: TLS version is: %s\n",
			gnutls_protocol_get_name(
				gnutls_protocol_get_version(session)));

	method = gnutls_compress_certificate_get_selected_method(session);
	if (method != GNUTLS_COMP_UNKNOWN)
		fail("server: compression method should not be set\n");

	if (server_bad)
		fail("server: certificate should not be compressed\n");

	ret = gnutls_certificate_verify_peers2(session, &status);
	if (ret < 0)
		fail("server: could not verify client certificate: %s\n",
		     gnutls_strerror(ret));
	if (status)
		fail("server: certificate verification failed\n");

	gnutls_bye(session, GNUTLS_SHUT_WR);

	if (debug)
		success("server: finished\n");

cleanup:
	close(fd);
	gnutls_deinit(session);
	gnutls_certificate_free_credentials(x509_cred);
	gnutls_global_deinit();
}

void doit(void)
{
	int fd[2];
	int ret;

	signal(SIGPIPE, SIG_IGN);

	ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
	if (ret < 0) {
		perror("socketpair");
		exit(1);
	}

	child = fork();
	if (child < 0) {
		perror("fork");
		fail("fork");
		exit(1);
	}

	if (child) {
		int status = 0;

		server(fd[0]);
		wait(&status);
		check_wait_status(status);
	} else {
		close(fd[0]);
		client(fd[1]);
		exit(0);
	}
}

#endif /* _WIN32 */
