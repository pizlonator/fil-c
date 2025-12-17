/*
 * Copyright (C) 2022 Free Software Foundation, Inc.
 *
 * Author: Fratnišek Krenželok
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
 * along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>
 *
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
#include <gnutls/crypto.h>
#include <gnutls/dtls.h>
#include <gnutls/socket.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

#include "cert-common.h"
#include "utils.h"

static void server_log_func(int level, const char *str)
{
	fprintf(stderr, "server|<%d>| %s", level, str);
}

static void client_log_func(int level, const char *str)
{
	fprintf(stderr, "client|<%d>| %s", level, str);
}

#define MAX_BUF 1024
#define MSG "Hello world!"
#define OFFSET 2

static void client(int fd, const char *prio)
{
	int ret;
	char buffer[MAX_BUF + 1];
	gnutls_certificate_credentials_t x509_cred;
	gnutls_session_t session;

	global_init();

	if (debug) {
		gnutls_global_set_log_function(client_log_func);
		gnutls_global_set_log_level(7);
	}

	gnutls_certificate_allocate_credentials(&x509_cred);

	gnutls_init(&session, GNUTLS_CLIENT);
	gnutls_handshake_set_timeout(session, 0);

	assert(gnutls_priority_set_direct(session, prio, NULL) >= 0);

	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred);

	gnutls_transport_set_int(session, fd);

	do {
		ret = gnutls_handshake(session);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

	if (ret < 0) {
		fail("client: Handshake failed\n");
		goto end;
	}
	if (debug)
		success("client: Handshake was completed\n");

	memset(buffer, 0, sizeof(buffer));
	do {
		ret = gnutls_record_recv(session, buffer, sizeof(buffer));
	} while (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED);

	if (ret == 0) {
		success("client: Peer has closed the TLS connection\n");
		goto end;
	} else if (ret < 0) {
		fail("client: Error: %s\n", gnutls_strerror(ret));
		goto end;
	}

	if (strncmp(buffer, MSG + OFFSET, ret)) {
		fail("client: Message doesn't match\n");
		goto end;
	}

	if (debug)
		success("client: messages received\n");

	ret = gnutls_bye(session, GNUTLS_SHUT_RDWR);
	if (ret < 0) {
		fail("client: error in closing session: %s\n",
		     gnutls_strerror(ret));
	}

	ret = 0;
end:

	close(fd);

	gnutls_deinit(session);

	gnutls_certificate_free_credentials(x509_cred);

	gnutls_global_deinit();

	if (ret != 0)
		exit(1);
}

static void server(int fd, const char *prio)
{
	int ret;
	gnutls_certificate_credentials_t x509_cred;
	gnutls_session_t session;

	global_init();

	if (debug) {
		gnutls_global_set_log_function(server_log_func);
		gnutls_global_set_log_level(7);
	}

	gnutls_certificate_allocate_credentials(&x509_cred);
	ret = gnutls_certificate_set_x509_key_mem(
		x509_cred, &server_cert, &server_key, GNUTLS_X509_FMT_PEM);
	if (ret < 0)
		exit(1);

	gnutls_init(&session, GNUTLS_SERVER);
	gnutls_handshake_set_timeout(session, 0);

	assert(gnutls_priority_set_direct(session, prio, NULL) >= 0);

	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred);

	gnutls_transport_set_int(session, fd);

	do {
		ret = gnutls_handshake(session);
	} while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

	if (ret < 0) {
		fail("server: Handshake has failed (%s)\n\n",
		     gnutls_strerror(ret));
		goto end;
	}

	if (debug)
		success("server: Handshake was completed\n");

	FILE *fp = tmpfile();
	if (fp == NULL) {
		fail("temporary file for testing couldn't be created");
		ret = gnutls_bye(session, GNUTLS_SHUT_RDWR);
		if (ret < 0)
			fail("server: error in closing session: %s\n",
			     gnutls_strerror(ret));
		goto end;
	}

	fputs(MSG, fp);
	rewind(fp);

	off_t offset = OFFSET;
	if (fp == NULL) {
		fail("server: couldn't open file for testing ...send_file() function");
		goto end;
	}

	do {
		ret = gnutls_record_send_file(session, fileno(fp), &offset,
					      512);
	} while (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED);

	if (ret < 0) {
		fail("server: sending file has failed (%s)\n\n",
		     gnutls_strerror(ret));
		goto end;
	}

	ret = gnutls_bye(session, GNUTLS_SHUT_RDWR);
	if (ret < 0)
		fail("server: error in closing session: %s\n",
		     gnutls_strerror(ret));

	ret = 0;
end:
	close(fd);
	gnutls_deinit(session);

	gnutls_certificate_free_credentials(x509_cred);

	gnutls_global_deinit();

	if (debug)
		success("server: finished\n");
}

static void run(const char *prio)
{
	int fd[2];
	int ret;
	pid_t child;

	success("testing with %s\n", prio);

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
		server(fd[0], prio);
	} else {
		close(fd[0]);
		client(fd[1], prio);
		exit(0);
	}
}

void doit(void)
{
	run("NORMAL:-VERS-ALL:+VERS-TLS1.2:-CIPHER-ALL:+AES-128-GCM");
	run("NORMAL:-VERS-ALL:+VERS-TLS1.2:-CIPHER-ALL:+AES-256-GCM");
	run("NORMAL:-VERS-ALL:+VERS-TLS1.3:-CIPHER-ALL:+AES-128-GCM");
	run("NORMAL:-VERS-ALL:+VERS-TLS1.3:-CIPHER-ALL:+AES-256-GCM");
}
#endif /* _WIN32 */
