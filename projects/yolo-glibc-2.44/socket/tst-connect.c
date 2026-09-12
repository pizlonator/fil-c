/* Test the connect function.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <support/check.h>
#include <support/xsocket.h>
#include <support/xunistd.h>
#include <sys/socket.h>
#include <stdio.h>

static struct sockaddr_in server_address;

int
open_socket_inet_tcp (void)
{
  int fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0)
    {
      if (errno == EAFNOSUPPORT)
        FAIL_UNSUPPORTED ("The host does not support IPv4");
      else
        FAIL_EXIT1 ("socket (AF_INET, SOCK_STREAM, IPPROTO_TCP): %m\n");
    }
  return fd;
}

static pid_t
start_server (void)
{
  server_address.sin_family = AF_INET;
  server_address.sin_port = 0;
  server_address.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  int server_sock = open_socket_inet_tcp ();

  xbind (server_sock, (struct sockaddr *) &server_address,
         sizeof (server_address));

  socklen_t sa_len = sizeof (server_address);
  xgetsockname (server_sock, (struct sockaddr *) &server_address, &sa_len);
  xlisten (server_sock, 5);

  pid_t my_pid = xfork ();
  if (my_pid > 0)
    {
      xclose (server_sock);
      return my_pid;
    }

  struct sockaddr_in client_address;
  socklen_t ca_len = sizeof (server_address);
  int client_sock = xaccept (server_sock, (struct sockaddr *) &client_address,
                             &ca_len);
  printf ("socket accepted %d\n", client_sock);

  _exit (0);
}

static int
do_test (void)
{
  pid_t serv_pid;
  struct sockaddr_in peer;
  socklen_t peer_len;

  serv_pid = start_server ();
  int client_sock = open_socket_inet_tcp ();
  xconnect (client_sock, (const struct sockaddr *) &server_address,
            sizeof (server_address));

  /* A second connect with same arguments should fail with EISCONN.  */
  int result = connect (client_sock,
                        (const struct sockaddr *) &server_address,
                        sizeof (server_address));
  if (result == 0 || errno != EISCONN)
    FAIL_EXIT1 ("Second connect (%d), should fail with EISCONN: %m",
                client_sock);

  peer_len = sizeof (peer);
  xgetpeername (client_sock, (struct sockaddr *) &peer, &peer_len);
  TEST_COMPARE (peer_len, sizeof (peer));
  TEST_COMPARE (peer.sin_port, server_address.sin_port);
  TEST_COMPARE_BLOB (&peer.sin_addr, sizeof (peer.sin_addr),
                     &server_address.sin_addr,
                     sizeof (server_address.sin_addr));

  int status;
  xwaitpid (serv_pid, &status, 0);
  TEST_COMPARE (status, 0);

  return 0;
}

#include <support/test-driver.c>
