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

#if defined(_WIN32) || !defined(HAVE_LIBZ)

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

/* This program tests whether the compress_certificate extension correctly fails
 * in the case of compression/decompression failure */

#define PRIO "NORMAL:-VERS-TLS-ALL:+VERS-TLS1.3"
#define CHECK(X) assert((X) >= 0)

static pid_t child;

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
	/* change compression method to BROTLI */
	msg->data[1] = 0x02;
	return 0;
}

static void client(int fd)
{
	int ret;
	gnutls_session_t session;
	gnutls_certificate_credentials_t x509_cred;
	gnutls_compression_method_t methods[] = { GNUTLS_COMP_ZLIB };
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
	if (ret < 0)
		gnutls_alert_send_appropriate(session, ret);
	else
		fail("client: handshake should have failed\n");

	gnutls_bye(session, GNUTLS_SHUT_WR);
	close(fd);
	gnutls_deinit(session);
	gnutls_certificate_free_credentials(x509_cred);
	gnutls_global_deinit();
}

static void server(int fd)
{
	int ret;
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

	gnutls_transport_set_int(session, fd);

	do {
		ret = gnutls_handshake(session);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
	if (ret >= 0)
		fail("server: handshake should have failed\n");

	if (gnutls_alert_get(session) != GNUTLS_A_BAD_CERTIFICATE)
		fail("server: didn't receive BAD CERTIFICATE alert\n");

	method = gnutls_compress_certificate_get_selected_method(session);
	if (method != GNUTLS_COMP_ZLIB)
		fail("server: compression method should be set to ZLIB\n");

	gnutls_bye(session, GNUTLS_SHUT_WR);
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
