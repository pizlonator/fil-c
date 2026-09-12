/* Test the shutdown function.
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
#include <stdbool.h>
#include <support/check.h>
#include <support/support.h>
#include <support/xsocket.h>
#include <support/xunistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

struct connection
{
  int sockets[2];
};

void
establish_connection (struct connection *conn)
{
  if (socketpair (AF_UNIX, SOCK_STREAM, 0, conn->sockets) != 0)
    {
      FAIL_EXIT1 ("socketpair (AF_UNIX, SOCK_STREAM, 0): %m\n");
    }
}

void
close_connection (struct connection *conn)
{
  xclose (conn->sockets[0]);
  xclose (conn->sockets[1]);
}

/* Open a file and check that shutdown fails with the ENOTSOCK error code.  */
void
do_test_enotsock (void)
{
  /* Open file and check that shutdown will fail with ENOTSOCK.  */
  int fd = xopen ("/dev/null", O_RDWR, 0);

  int result = shutdown (fd, SHUT_RD);
  if (result == 0 || errno != ENOTSOCK)
    {
      FAIL_EXIT1 ("shutdown should fail with ENOTSOCK");
    }
  xclose (fd);
}

/* Test shutdown with SHUT_RD.  */
void
do_test_shut_rd (void)
{
  struct connection conn;
  const char *str = "AAAAAAA";
  int len = 8;
  int ret;
  void *s_buf = xmalloc (len);
  bzero (s_buf, len);

  establish_connection (&conn);
  int server = conn.sockets[0];
  int client = conn.sockets[1];

  /* Call shutdown with SHUT_RD on server socket.  */
  if (shutdown (server, SHUT_RD) != 0)
    {
      FAIL_EXIT1 ("shutdown with SHUT_RD on socket %d failed", server);
    }

  ret = send (server, str, len, 0);
  if (ret <= 0)
    {
      FAIL_EXIT1 ("send (%d, data, %d): %m", server, len);
    }

  ret = recv (client, s_buf, len, 0);
  if (ret <= 0)
    {
      FAIL_EXIT1 ("recv (%d, data, %d): %m", client, len);
    }

  TEST_COMPARE_BLOB (str, len, s_buf, len);

  /* Send data should be disallowed on shutdown socket.  */
  errno = 0;
  ret = send (client, str, len, MSG_NOSIGNAL);
  if (ret >= 0 || errno != EPIPE)
    {
      FAIL_EXIT1 ("Send on SHUT_RD socket should be disallowed: %m");
    }

  /* Recv should return zero and no error.  */
  errno = 0;
  ret = recv (server, s_buf, len, 0);
  if (ret != 0 || errno != 0)
    {
      FAIL_EXIT1 ("recv should return 0 without error: %m");
    }

  close_connection (&conn);
}

/* Test shutdown with SHUT_WR.  */
void
do_test_shut_wr (void)
{
  struct connection conn;
  const char *str1 = "CCCCCCC";
  const char *str2 = "DDDDDDD";
  const char *str3 = "EEEEEEE";
  int len = 8;
  int ret;
  void *c_buf = xmalloc (len);
  void *s_buf = xmalloc (len);

  establish_connection (&conn);
  int server = conn.sockets[0];
  int client = conn.sockets[1];

  xwrite (client, str1, len);

  if (shutdown (client, SHUT_WR) != 0)
    {
      FAIL_EXIT1 ("shutdown with SHUT_WR on socket %d failed", client);
    }

  ret = send (client, str2, len, MSG_NOSIGNAL);
  if (ret >= 0)
    {
      FAIL_EXIT1 ("send on SHUT_WR socket should fail");
    }

  /* Read data written before shutdown and check if it's correct.  */
  xread (server, s_buf, len);
  TEST_COMPARE_BLOB (str1, len, s_buf, len);

  /* Second read should return zero without error.  */
  errno = 0;
  if (read (server, s_buf, len) != 0 || errno != 0)
    {
      FAIL_EXIT1 ("read after shutdown should return zero without error: %m");
    }

  /* Write some data to socket and check it still can be read on other side.  */
  memcpy (s_buf, str3, len);
  xwrite (server, s_buf, len);

  xread (client, c_buf, len);
  TEST_COMPARE_BLOB (s_buf, len, c_buf, len);

  close_connection (&conn);
}

/* Test shutdown with SHUT_RDWR.  */
void
do_test_shut_rdwr (void)
{
  struct connection conn;
  struct sockaddr peer;
  socklen_t peer_len = sizeof (peer);

  const char *str1 = "FFFFFFF";
  const char *str2 = "GGGGGGG";
  int len = 8;
  int ret;
  void *s_buf = xmalloc (len);
  bzero (s_buf, len);

  establish_connection (&conn);
  int server = conn.sockets[0];
  int client = conn.sockets[1];

  /* Send some data to both sockets before shutdown.  */
  xwrite (client, str1, len);
  xwrite (server, str2, len);

  /* Call shutdown with SHUT_RDWR on client socket.  */
  if (shutdown (client, SHUT_RDWR) != 0)
    {
      FAIL_EXIT1 ("shutdown with SHUT_RDWR on socket %d failed", client);
    }

  /* Verify that socket is still connected.  */
  xgetsockname (client, &peer, &peer_len);

  /* Read data written before shutdown.  */
  xread (client, s_buf, len);
  TEST_COMPARE_BLOB (s_buf, len, str2, len);

  /* Second read should return zero, but no error.  */
  errno = 0;
  if (read (client, s_buf, len) != 0 || errno != 0)
    {
      FAIL_EXIT1 ("read after shutdown should return zero without error: %m");
    }

  /* Send some data to shutdown socket and expect error.  */
  errno = 0;
  ret = send (server, str2, len, MSG_NOSIGNAL);
  if (ret >= 0 || errno != EPIPE)
    {
      FAIL_EXIT1 ("send to RDWR shutdown socket should fail with EPIPE");
    }

  /* Read data written before shutdown.  */
  xread (server, s_buf, len);
  TEST_COMPARE_BLOB (s_buf, len, str1, len);

  /* Second read should return zero, but no error.  */
  errno = 0;
  if (read (server, s_buf, len) != 0 || errno != 0)
    {
      FAIL_EXIT1 ("read after shutdown should return zero without error: %m");
    }

  /* Send some data to shutdown socket and expect error.  */
  errno = 0;
  ret = send (client, str1, len, MSG_NOSIGNAL);
  if (ret >= 0 || errno != EPIPE)
    {
      FAIL_EXIT1 ("send to RDWR shutdown socket should fail with EPIPE");
    }

  close_connection (&conn);
}

static int
do_test (void)
{
  do_test_enotsock ();
  do_test_shut_rd ();
  do_test_shut_wr ();
  do_test_shut_rdwr ();

  return 0;
}

#include <support/test-driver.c>
