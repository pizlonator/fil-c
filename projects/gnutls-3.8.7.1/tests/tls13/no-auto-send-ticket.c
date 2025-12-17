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

static unsigned tickets_seen = 0;
static int ticket_callback(gnutls_session_t session, unsigned int htype,
			   unsigned post, unsigned int incoming,
			   const gnutls_datum_t *msg)
{
	gnutls_datum *d;
	int ret;

	assert(htype == GNUTLS_HANDSHAKE_NEW_SESSION_TICKET);

	d = gnutls_session_get_ptr(session);

	if (post == GNUTLS_HOOK_POST) {
		tickets_seen++;
		if (d->data)
			gnutls_free(d->data);
		ret = gnutls_session_get_data2(session, d);
		assert(ret >= 0);
		assert(d->size > 4);

		return 0;
	}

	return 0;
}

static void client(int fd, unsigned flags, unsigned tickets)
{
	int ret;
	gnutls_session_t session;
	unsigned try = 0;
	gnutls_datum_t session_data = { NULL, 0 };
	gnutls_certificate_credentials_t x509_cred;

	global_init();
	tickets_seen = 0;

	if (debug) {
		gnutls_global_set_log_function(client_log_func);
		gnutls_global_set_log_level(7);
	}

	assert(gnutls_certificate_allocate_credentials(&x509_cred) >= 0);

retry:
	/* Initialize TLS session
	 */
	assert(gnutls_init(&session, GNUTLS_CLIENT | flags) >= 0);

	gnutls_handshake_set_timeout(session, get_timeout());

	ret = gnutls_priority_set_direct(
		session,
		"NORMAL:-VERS-ALL:+VERS-TLS1.3:+VERS-TLS1.2:+VERS-TLS1.0",
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

	if (tickets_seen != tickets)
		fail("unexpected number of tickets received: %u != %u",
		     tickets_seen, tickets);

	gnutls_deinit(session);

	if (tickets > 0 && try == 0) {
		try++;
		goto retry;
	}

	close(fd);
	gnutls_certificate_free_credentials(x509_cred);
	gnutls_free(session_data.data);

	gnutls_global_deinit();
}

static void server(int fd, unsigned flags, unsigned tickets_sent,
		   unsigned tickets_expected)
{
	int ret;
	gnutls_session_t session;
	gnutls_certificate_credentials_t x509_cred;
	gnutls_datum_t skey;

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

	assert(gnutls_init(&session, GNUTLS_SERVER | flags) >= 0);

	assert(gnutls_session_ticket_enable_server(session, &skey) >= 0);
	gnutls_handshake_set_timeout(session, get_timeout());

	assert(gnutls_priority_set_direct(session, "NORMAL:+VERS-TLS1.3",
					  NULL) >= 0);

	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred);

	gnutls_transport_set_int(session, fd);

	do {
		ret = gnutls_handshake(session);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

	if (ret != 0)
		fail("handshake failed: %s\n", gnutls_strerror(ret));

	if (tickets_sent > 0) {
		do {
			ret = gnutls_session_ticket_send(session, tickets_sent,
							 0);
		} while (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED);
	}

	do {
		ret = gnutls_bye(session, GNUTLS_SHUT_RDWR);
	} while (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED);
	gnutls_deinit(session);

	if (tickets_expected > 0) {
		/* resume session
		 */
		assert(gnutls_init(&session, GNUTLS_SERVER | flags) >= 0);

		assert(gnutls_session_ticket_enable_server(session, &skey) >=
		       0);
		gnutls_handshake_set_timeout(session, get_timeout());
		assert(gnutls_priority_set_direct(
			       session, "NORMAL:+VERS-TLS1.3", NULL) >= 0);

		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
				       x509_cred);

		gnutls_transport_set_int(session, fd);

		do {
			ret = gnutls_handshake(session);
		} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

		if (ret != 0)
			fail("handshake failed: %s\n", gnutls_strerror(ret));

		assert(gnutls_session_is_resumed(session) != 0);

		gnutls_bye(session, GNUTLS_SHUT_RDWR);
		gnutls_deinit(session);
	}

	gnutls_free(skey.data);
	close(fd);
	gnutls_certificate_free_credentials(x509_cred);

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

static void start(const char *name, unsigned flags, unsigned tickets_sent,
		  unsigned tickets_expected)
{
	int fd[2];
	int ret;
	pid_t child;

	success("testing: %s\n", name);

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
		server(fd[0], flags, tickets_sent, tickets_expected);
		kill(child, SIGTERM);
	} else {
		close(fd[0]);
		client(fd[1], flags, tickets_expected);
		exit(0);
	}
}

void doit(void)
{
	signal(SIGCHLD, ch_handler);
	signal(SIGPIPE, SIG_IGN);

	start("auto send ticket 0", 0, 0, TLS13_TICKETS_TO_SEND);
	start("auto send ticket 1", 0, 1, TLS13_TICKETS_TO_SEND + 1);
	start("no auto send ticket 0", GNUTLS_NO_AUTO_SEND_TICKET, 0, 0);
	start("no auto send ticket 1", GNUTLS_NO_AUTO_SEND_TICKET, 1, 1);
}
#endif /* _WIN32 */
