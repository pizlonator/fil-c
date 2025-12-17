// Copyright (C) 2022 Red Hat, Inc.
//
// Author: Frantisek Krenzelok
//
// This file is part of GnuTLS.
//
// GnuTLS is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3 of the License, or (at
// your option) any later version.
//
// GnuTLS is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with GnuTLS.  If not, see <https://www.gnu.org/licenses/>.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
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

#if defined(_WIN32)

int main(void)
{
	exit(77);
}

#else

#define MAX_BUF 1024
#define MSG "Hello world!"

#define HANDSHAKE(session, name, ret)                                 \
	{                                                             \
		do {                                                  \
			ret = gnutls_handshake(session);              \
		} while (ret < 0 && gnutls_error_is_fatal(ret) == 0); \
		if (ret < 0) {                                        \
			fail("%s: Handshake failed\n", name);         \
			goto end;                                     \
		}                                                     \
	}

#define SEND_MSG(session, name, ret)                                     \
	{                                                                \
		do {                                                     \
			ret = gnutls_record_send(session, MSG,           \
						 strlen(MSG) + 1);       \
		} while (ret == GNUTLS_E_AGAIN ||                        \
			 ret == GNUTLS_E_INTERRUPTED);                   \
		if (ret < 0) {                                           \
			fail("%s: data sending has failed (%s)\n", name, \
			     gnutls_strerror(ret));                      \
			goto end;                                        \
		}                                                        \
	}

#define RECV_MSG(session, name, buffer, buffer_len, ret)                       \
	{                                                                      \
		memset(buffer, 0, sizeof(buffer));                             \
		do {                                                           \
			ret = gnutls_record_recv(session, buffer,              \
						 sizeof(buffer));              \
		} while (ret == GNUTLS_E_AGAIN ||                              \
			 ret == GNUTLS_E_INTERRUPTED);                         \
		if (ret == 0) {                                                \
			success("%s: Peer has closed the TLS connection\n",    \
				name);                                         \
			goto end;                                              \
		} else if (ret < 0) {                                          \
			fail("%s: Error -> %s\n", name, gnutls_strerror(ret)); \
			goto end;                                              \
		}                                                              \
		if (strncmp(buffer, MSG, ret)) {                               \
			fail("%s: Message doesn't match\n", name);             \
			goto end;                                              \
		}                                                              \
	}

#define KEY_UPDATE(session, name, peer_req, ret)                            \
	{                                                                   \
		do {                                                        \
			ret = gnutls_session_key_update(session, peer_req); \
		} while (ret == GNUTLS_E_AGAIN ||                           \
			 ret == GNUTLS_E_INTERRUPTED);                      \
		if (ret < 0) {                                              \
			fail("%s: key update has failed (%s)\n", name,      \
			     gnutls_strerror(ret));                         \
			goto end;                                           \
		}                                                           \
	}

#define CHECK_KTLS_ENABLED(session, ret)                                     \
	{                                                                    \
		ret = gnutls_transport_is_ktls_enabled(session);             \
		if (!(ret & GNUTLS_KTLS_RECV)) {                             \
			fail("client: KTLS was not properly initialized\n"); \
			goto end;                                            \
		}                                                            \
	}

static void server_log_func(int level, const char *str)
{
	fprintf(stderr, "server|<%d>| %s", level, str);
}

static void client_log_func(int level, const char *str)
{
	fprintf(stderr, "client|<%d>| %s", level, str);
}

static void client(int fd, const char *prio, int pipe)
{
	const char *name = "client";
	int ret;
	char foo;
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

	HANDSHAKE(session, name, ret);

	CHECK_KTLS_ENABLED(session, ret)
	// Test 0: Try sending/receiving data
	RECV_MSG(session, name, buffer, MAX_BUF + 1, ret)
	SEND_MSG(session, name, ret)
	CHECK_KTLS_ENABLED(session, ret)
	// Test 1: Servers does key update
	read(pipe, &foo, 1);
	RECV_MSG(session, name, buffer, MAX_BUF + 1, ret)
	SEND_MSG(session, name, ret)
	CHECK_KTLS_ENABLED(session, ret)
	// Test 2: Does key update witch request
	read(pipe, &foo, 1);
	RECV_MSG(session, name, buffer, MAX_BUF + 1, ret)
	SEND_MSG(session, name, ret)
	CHECK_KTLS_ENABLED(session, ret)
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

pid_t child;
static void terminate(void)
{
	assert(child);
	kill(child, SIGTERM);
	exit(1);
}

static void server(int fd, const char *prio, int pipe)
{
	const char *name = "server";
	int ret;
	char bar = 0;
	char buffer[MAX_BUF + 1];
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

	HANDSHAKE(session, name, ret)
	CHECK_KTLS_ENABLED(session, ret)
	success("Test 0: sending/receiving data\n");
	SEND_MSG(session, name, ret)
	RECV_MSG(session, name, buffer, MAX_BUF + 1, ret)
	CHECK_KTLS_ENABLED(session, ret)
	success("Test 1: server key update without request\n");
	KEY_UPDATE(session, name, 0, ret)
	write(pipe, &bar, 1);
	SEND_MSG(session, name, ret)
	RECV_MSG(session, name, buffer, MAX_BUF + 1, ret)
	CHECK_KTLS_ENABLED(session, ret)
	success("Test 2: server key update with request\n");
	KEY_UPDATE(session, name, GNUTLS_KU_PEER, ret)
	write(pipe, &bar, 1);
	SEND_MSG(session, name, ret)
	RECV_MSG(session, name, buffer, MAX_BUF + 1, ret)
	CHECK_KTLS_ENABLED(session, ret)
	ret = gnutls_bye(session, GNUTLS_SHUT_RDWR);
	if (ret < 0) {
		fail("server: error in closing session: %s\n",
		     gnutls_strerror(ret));
	}

	ret = 0;
end:
	close(fd);
	gnutls_deinit(session);

	gnutls_certificate_free_credentials(x509_cred);

	gnutls_global_deinit();

	if (ret) {
		terminate();
	}

	if (debug)
		success("server: finished\n");
}

static void ch_handler(int sig)
{
	return;
}

static void run(const char *prio)
{
	int ret;
	struct sockaddr_in saddr;
	socklen_t addrlen;
	int listener;
	int fd;

	int sync_pipe[2]; //used for synchronization
	pipe(sync_pipe);

	success("running ktls test with %s\n", prio);

	signal(SIGCHLD, ch_handler);
	signal(SIGPIPE, SIG_IGN);

	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == -1) {
		fail("error in listener(): %s\n", strerror(errno));
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	saddr.sin_port = 0;

	ret = bind(listener, (struct sockaddr *)&saddr, sizeof(saddr));
	if (ret == -1) {
		fail("error in bind(): %s\n", strerror(errno));
	}

	addrlen = sizeof(saddr);
	ret = getsockname(listener, (struct sockaddr *)&saddr, &addrlen);
	if (ret == -1) {
		fail("error in getsockname(): %s\n", strerror(errno));
	}

	child = fork();
	if (child < 0) {
		fail("error in fork(): %s\n", strerror(errno));
		exit(1);
	}

	if (child) {
		int status;
		/* parent */
		ret = listen(listener, 1);
		if (ret == -1) {
			fail("error in listen(): %s\n", strerror(errno));
		}

		fd = accept(listener, NULL, NULL);
		if (fd == -1) {
			fail("error in accept(): %s\n", strerror(errno));
		}

		close(sync_pipe[0]);
		server(fd, prio, sync_pipe[1]);

		wait(&status);
		check_wait_status(status);
	} else {
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd == -1) {
			fail("error in socket(): %s\n", strerror(errno));
			exit(1);
		}

		usleep(1000000);
		connect(fd, (struct sockaddr *)&saddr, addrlen);

		close(sync_pipe[1]);
		client(fd, prio, sync_pipe[0]);
		exit(0);
	}
}

void doit(void)
{
	run("NORMAL:-VERS-ALL:+VERS-TLS1.3:-CIPHER-ALL:+AES-128-GCM");
	run("NORMAL:-VERS-ALL:+VERS-TLS1.3:-CIPHER-ALL:+AES-256-GCM");
}

#endif /* _WIN32 */
