/* Smoke test for MSG_CMSG_CLOEXEC.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <support/xunistd.h>
#include <support/check.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <fcntl.h>

static void
send_fd (int sockfd, int fd)
{
  char data[] = "hello";
  struct iovec iov = { .iov_base = data, .iov_len = sizeof (data) };

  union
  {
    struct cmsghdr header;
    char bytes[CMSG_SPACE (sizeof (fd))];
  } cmsg_storage;

  struct msghdr msg =
    {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = cmsg_storage.bytes,
      .msg_controllen = sizeof (cmsg_storage)
    };

  memset (&cmsg_storage, 0, sizeof (cmsg_storage));

  struct cmsghdr *cmsg = CMSG_FIRSTHDR (&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN (sizeof (fd));
  memcpy (CMSG_DATA (cmsg), &fd, sizeof (fd));

  ssize_t nsent = sendmsg (sockfd, &msg, 0);
  if (nsent < 0)
    FAIL_EXIT1 ("sendmsg (%d): %m", sockfd);
  TEST_COMPARE (nsent, sizeof (data));
}

static int
recv_fd (int sockfd, int flags)
{
  char buffer[100];
  struct iovec iov = { .iov_base = buffer, .iov_len = sizeof (buffer) };

  union
  {
    struct cmsghdr header;
    char bytes[100];
  } cmsg_storage;

  struct msghdr msg =
    {
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = cmsg_storage.bytes,
      .msg_controllen = sizeof (cmsg_storage)
    };

  ssize_t nrecv = recvmsg (sockfd, &msg, flags);
  if (nrecv < 0)
    FAIL_EXIT1 ("recvmsg (%d): %m", sockfd);

  TEST_COMPARE (msg.msg_controllen, CMSG_SPACE (sizeof (int)));
  struct cmsghdr *cmsg = CMSG_FIRSTHDR (&msg);
  TEST_COMPARE (cmsg->cmsg_level, SOL_SOCKET);
  TEST_COMPARE (cmsg->cmsg_type, SCM_RIGHTS);
  TEST_COMPARE (cmsg->cmsg_len, CMSG_LEN (sizeof (int)));

  int fd;
  memcpy (&fd, CMSG_DATA (cmsg), sizeof (fd));
  return fd;
}

static int
do_test (void)
{
  int sockfd[2];
  int newfd;
  int flags;
  int rc = socketpair (AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0, sockfd);
  if (rc < 0)
    FAIL_EXIT1 ("socketpair: %m");

  send_fd (sockfd[1], STDIN_FILENO);
  newfd = recv_fd (sockfd[0], 0);
  TEST_VERIFY_EXIT (newfd > 0);
  flags = fcntl (newfd, F_GETFD, 0);
  TEST_VERIFY_EXIT (flags != -1);
  TEST_VERIFY (!(flags & FD_CLOEXEC));
  xclose (newfd);

  send_fd (sockfd[1], STDIN_FILENO);
  newfd = recv_fd (sockfd[0], MSG_CMSG_CLOEXEC);
  TEST_VERIFY_EXIT (newfd > 0);
  flags = fcntl (newfd, F_GETFD, 0);
  TEST_VERIFY_EXIT (flags != -1);
  TEST_VERIFY (flags & FD_CLOEXEC);
  xclose (newfd);

  xclose (sockfd[0]);
  xclose (sockfd[1]);
  return 0;
}

#include <support/test-driver.c>
