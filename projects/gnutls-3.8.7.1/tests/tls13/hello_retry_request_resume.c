/*
 * Copyright (C) 2017-2020 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

int main(void)
{
	exit(77);
}

#else

#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <signal.h>
#include <assert.h>

#include "../lib/handshake-defs.h"
#include "cert-common.h"
#include "utils.h"

/* This program tests whether the certificate seen in Post Handshake Auth
 * is found in a resumed session under TLS 1.3.
 */

static void server_log_func(int level, const char *str)
{
	fprintf(stderr, "server|<%d>| %s", level, str);
}

static void client_log_func(int level, const char *str)
{
	fprintf(stderr, "client|<%d>| %s", level, str);
}

static int ticket_callback(gnutls_session_t session, unsigned int htype,
			   unsigned post, unsigned int incoming,
			   const gnutls_datum_t *msg)
{
	gnutls_datum *d;
	int ret;

	assert(htype == GNUTLS_HANDSHAKE_NEW_SESSION_TICKET);

	d = gnutls_session_get_ptr(session);

	if (post == GNUTLS_HOOK_POST) {
		if (d->data)
			gnutls_free(d->data);
		ret = gnutls_session_get_data2(session, d);
		assert(ret >= 0);
		assert(d->size > 4);

		return 0;
	}

	return 0;
}

static void client(int fd)
{
	int ret;
	gnutls_session_t session;
	unsigned try = 0;
	gnutls_datum_t session_data = { NULL, 0 };
	gnutls_certificate_credentials_t x509_cred;

	global_init();

	if (debug) {
		gnutls_global_set_log_function(client_log_func);
		gnutls_global_set_log_level(7);
	}

	assert(gnutls_certificate_allocate_credentials(&x509_cred) >= 0);

retry:
	/* Initialize TLS session
	 */
	assert(gnutls_init(&session, GNUTLS_CLIENT) >= 0);

	gnutls_handshake_set_timeout(session, get_timeout());

	ret = gnutls_priority_set_direct(
		session,
		"NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-SECP256R1:+GROUP-X25519",
		NULL);
	if (ret < 0)
		fail("cannot set TLS 1.3 priorities\n");

	if (try == 0) {
		gnutls_session_set_ptr(session, &session_data);
		gnutls_handshake_set_hook_function(
			session, GNUTLS_HANDSHAKE_NEW_SESSION_TICKET,
			GNUTLS_HOOK_BOTH, ticket_callback);
	} else {
		assert(gnutls_session_set_data(session, session_data.data,
					       session_data.size) >= 0);
	}

	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred);

	gnutls_transport_set_int(session, fd);

	/* Perform the TLS handshake
	 */
	do {
		ret = gnutls_handshake(session);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

	if (ret != 0)
		fail("handshake failed: %s\n", gnutls_strerror(ret));

	do {
		ret = gnutls_bye(session, GNUTLS_SHUT_RDWR);
	} while (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED);

	if (ret != 0) {
		fail("error in recv: %s\n", gnutls_strerror(ret));
	}

	gnutls_deinit(session);

	if (try == 0) {
		try++;
		goto retry;
	}

	gnutls_free(session_data.data);
	close(fd);
	gnutls_certificate_free_credentials(x509_cred);

	gnutls_global_deinit();
}

#define HANDSHAKE_SESSION_ID_POS 34

static int client_hello_callback(gnutls_session_t session, unsigned int htype,
				 unsigned post, unsigned int incoming,
				 const gnutls_datum_t *msg)
{
	gnutls_datum *d;

	assert(post == GNUTLS_HOOK_POST);
	assert(msg->size >= HANDSHAKE_SESSION_ID_POS + 1);

	d = gnutls_session_get_ptr(session);
	d->size = msg->data[HANDSHAKE_SESSION_ID_POS];
	d->data = gnutls_malloc(d->size);
	memcpy(d->data, &msg->data[HANDSHAKE_SESSION_ID_POS], d->size);

	return 0;
}

static void server(int fd)
{
	int ret;
	gnutls_session_t session;
	unsigned try = 0;
	gnutls_certificate_credentials_t x509_cred;
	gnutls_datum_t skey;
	gnutls_datum_t session_id = { NULL, 0 };
	gnutls_datum_t retry_session_id = { NULL, 0 };

	/* this must be called once in the program
	 */
	global_init();

	assert(gnutls_session_ticket_key_generate(&skey) >= 0);

	if (debug) {
		gnutls_global_set_log_function(server_log_func);
		gnutls_global_set_log_level(4711);
	}

	gnutls_certificate_allocate_credentials(&x509_cred);
	gnutls_certificate_set_x509_key_mem(x509_cred, &server_cert,
					    &server_key, GNUTLS_X509_FMT_PEM);

retry:
	assert(gnutls_init(&session, GNUTLS_SERVER) >= 0);

	assert(gnutls_session_ticket_enable_server(session, &skey) >= 0);
	gnutls_handshake_set_timeout(session, get_timeout());

	/* server only supports x25519, client advertises secp256r1 */
	assert(gnutls_priority_set_direct(
		       session,
		       "NORMAL:-VERS-ALL:+VERS-TLS1.3:-GROUP-ALL:+GROUP-X25519",
		       NULL) >= 0);

	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred);

	gnutls_transport_set_int(session, fd);

	if (try == 0) {
		gnutls_session_set_ptr(session, &session_id);
	} else {
		gnutls_session_set_ptr(session, &retry_session_id);
	}

	gnutls_handshake_set_hook_function(session,
					   GNUTLS_HANDSHAKE_CLIENT_HELLO,
					   GNUTLS_HOOK_POST,
					   client_hello_callback);

	do {
		ret = gnutls_handshake(session);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

	if (ret != 0)
		fail("handshake failed: %s\n", gnutls_strerror(ret));

	if (try > 0) {
		assert(gnutls_session_is_resumed(session) != 0);

		/* Check that the same (non-empty) session ID is used in both
		 * initial and resumption handshakes.  This assumes
		 * TLS13_APPENDIX_D4 is set to 1 in lib/handshake-defs.h. Once
		 * it's turned off, both session IDs should be empty. */
		if (session_id.size == 0 ||
		    session_id.size != retry_session_id.size ||
		    memcmp(session_id.data, retry_session_id.data,
			   session_id.size)) {
			fail("session ids are different after resumption: %u, %u\n",
			     session_id.size, retry_session_id.size);
		}
	}

	do {
		ret = gnutls_bye(session, GNUTLS_SHUT_RDWR);
	} while (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED);
	gnutls_deinit(session);

	if (try == 0) {
		try++;
		goto retry;
	}

	gnutls_free(skey.data);
	close(fd);
	gnutls_certificate_free_credentials(x509_cred);
	gnutls_free(session_id.data);
	gnutls_free(retry_session_id.data);

	gnutls_global_deinit();

	if (debug)
		success("server: client/server hello were verified\n");
}

static void ch_handler(int sig)
{
	int status = 0;
	wait(&status);
	check_wait_status(status);
	return;
}

void doit(void)
{
	int fd[2];
	int ret;
	pid_t child;

	signal(SIGCHLD, ch_handler);
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
		/* parent */
		close(fd[1]);
		server(fd[0]);
		kill(child, SIGTERM);
	} else {
		close(fd[0]);
		client(fd[1]);
		exit(0);
	}
}
#endif /* _WIN32 */
