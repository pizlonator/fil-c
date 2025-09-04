/* Check recvmsg/recvmmsg 64-bit timestamp support.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
#include <string.h>
#include <support/check.h>
#include <support/xsocket.h>
#include <support/xunistd.h>
#include <stdbool.h>
#include <socket-constants-time64.h>

/* AF_INET socket and address used to receive data.  */
static int srv;
static struct sockaddr_in srv_addr;

static int
do_sendto (const struct sockaddr_in *addr, int payload)
{
  int s = xsocket (AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  xconnect (s, (const struct sockaddr *) addr, sizeof (*addr));

  xsendto (s, &payload, sizeof (payload), 0, (const struct sockaddr *) addr,
	   sizeof (*addr));

  xclose (s);

  return 0;
}

static void
do_recvmsg_ancillary (bool use_multi_call, struct mmsghdr *mmhdr,
		      void *msgbuf, size_t msgbuflen, int exp_payload)
{
  int payload;
  struct iovec iov =
    {
      .iov_base = &payload,
      .iov_len = sizeof (payload)
    };
  mmhdr->msg_hdr.msg_name = NULL;
  mmhdr->msg_hdr.msg_iov = &iov;
  mmhdr->msg_hdr.msg_iovlen = 1;
  mmhdr->msg_hdr.msg_control = msgbuf;
  mmhdr->msg_hdr.msg_controllen = msgbuflen;

  int r;
  if (use_multi_call)
    {
      r = recvmmsg (srv, mmhdr, 1, 0, NULL);
      if (r >= 0)
	r = mmhdr->msg_len;
    }
  else
    r = recvmsg (srv, &mmhdr->msg_hdr, 0);
  TEST_COMPARE (r, sizeof (int));
  TEST_COMPARE (payload, exp_payload);
}

/* Check if recvmsg create the additional 64 bit timestamp if only 32 bit
   is enabled for 64 bit recvmsg symbol.  */
static void
do_test_large_buffer (bool mc)
{
  struct mmsghdr mmhdr = { 0 };
  /* It should be large enough for either timeval/timespec and the
     64 time type as well.  */

  union
  {
    struct cmsghdr cmsghdr;
    char msgbuf[512];
  } control;

  /* Enable 32 bit timeval precision and check if no 64 bit timeval stamp
     is created.  */
  {
    int r = setsockopt (srv, SOL_SOCKET, COMPAT_SO_TIMESTAMP_OLD, &(int){1},
			sizeof (int));
    TEST_VERIFY_EXIT (r != -1);

    do_sendto (&srv_addr, 42);
    do_recvmsg_ancillary (mc, &mmhdr, &control, sizeof control, 42);

    bool found_timestamp = false;
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&mmhdr.msg_hdr);
	 cmsg != NULL;
	 cmsg = CMSG_NXTHDR (&mmhdr.msg_hdr, cmsg))
    {
      if (cmsg->cmsg_level != SOL_SOCKET)
	continue;

      if (sizeof (time_t) > 4 && cmsg->cmsg_type == COMPAT_SO_TIMESTAMP_NEW)
	found_timestamp = true;
      else
	TEST_VERIFY (cmsg->cmsg_type != COMPAT_SO_TIMESTAMP_NEW);
    }

    TEST_COMPARE (found_timestamp, sizeof (time_t) > 4);
  }

  /* Same as before, but for timespec.  */
  {
    int r = setsockopt (srv, SOL_SOCKET, COMPAT_SO_TIMESTAMPNS_OLD, &(int){1},
			sizeof (int));
    TEST_VERIFY_EXIT (r != -1);

    do_sendto (&srv_addr, 42);
    do_recvmsg_ancillary (mc, &mmhdr, &control, sizeof control, 42);

    bool found_timestamp = false;
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&mmhdr.msg_hdr);
	 cmsg != NULL;
	 cmsg = CMSG_NXTHDR (&mmhdr.msg_hdr, cmsg))
    {
      if (cmsg->cmsg_level != SOL_SOCKET)
	continue;

      if (sizeof (time_t) > 4 && cmsg->cmsg_type == COMPAT_SO_TIMESTAMPNS_NEW)
	found_timestamp = true;
      else
	TEST_VERIFY (cmsg->cmsg_type != COMPAT_SO_TIMESTAMPNS_NEW);
    }

    TEST_COMPARE (found_timestamp, sizeof (time_t) > 4);
  }
}

/* Check if recvmsg does not create the additional 64 bit timestamp if
   only 32 bit timestamp is enabled if the ancillary buffer is not large
   enough.  Also checks if MSG_CTRUNC is set iff for 64 bit recvmsg
   symbol.  */
static void
do_test_small_buffer (bool mc)
{
  struct mmsghdr mmhdr = { 0 };

  /* Enable 32 bit timeval precision and check if no 64 bit timeval stamp
     is created.  */
  {
    int r = setsockopt (srv, SOL_SOCKET, COMPAT_SO_TIMESTAMP_OLD, &(int){1},
			sizeof (int));
    TEST_VERIFY_EXIT (r != -1);

    union
    {
      struct cmsghdr cmsghdr;
      char msgbuf[CMSG_SPACE (sizeof (struct timeval))];
    } control;

    do_sendto (&srv_addr, 42);
    do_recvmsg_ancillary (mc, &mmhdr, &control, sizeof control, 42);

    bool found_timestamp = false;
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&mmhdr.msg_hdr);
	 cmsg != NULL;
	 cmsg = CMSG_NXTHDR (&mmhdr.msg_hdr, cmsg))
    {
      if (cmsg->cmsg_level != SOL_SOCKET)
	continue;

      if (sizeof (time_t) > 4 && cmsg->cmsg_type == COMPAT_SO_TIMESTAMP_NEW)
	found_timestamp = true;
      else
	TEST_VERIFY (cmsg->cmsg_type != COMPAT_SO_TIMESTAMP_NEW);
    }

    if (sizeof (time_t) > 4)
      {
	TEST_VERIFY ((mmhdr.msg_hdr.msg_flags & MSG_CTRUNC));
	TEST_COMPARE (found_timestamp, 0);
      }
    else
      {
	TEST_VERIFY (!(mmhdr.msg_hdr.msg_flags & MSG_CTRUNC));
	TEST_COMPARE (found_timestamp, 0);
      }
  }

  /* Same as before, but for timespec.  */
  {
    int r = setsockopt (srv, SOL_SOCKET, COMPAT_SO_TIMESTAMPNS_OLD, &(int){1},
			sizeof (int));
    TEST_VERIFY_EXIT (r != -1);

    union
    {
      struct cmsghdr cmsghdr;
      char msgbuf[CMSG_SPACE (sizeof (struct timespec))];
    } control;

    do_sendto (&srv_addr, 42);
    do_recvmsg_ancillary (mc, &mmhdr, &control, sizeof control, 42);

    bool found_timestamp = false;
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&mmhdr.msg_hdr);
	 cmsg != NULL;
	 cmsg = CMSG_NXTHDR (&mmhdr.msg_hdr, cmsg))
    {
      if (cmsg->cmsg_level != SOL_SOCKET)
	continue;

      if (sizeof (time_t) > 4 && cmsg->cmsg_type == COMPAT_SO_TIMESTAMPNS_NEW)
	found_timestamp = true;
      else
	TEST_VERIFY (cmsg->cmsg_type != COMPAT_SO_TIMESTAMPNS_NEW);
    }

    if (sizeof (time_t) > 4)
      {
	TEST_VERIFY ((mmhdr.msg_hdr.msg_flags & MSG_CTRUNC));
	TEST_COMPARE (found_timestamp, 0);
      }
    else
      {
	TEST_VERIFY ((mmhdr.msg_hdr.msg_flags & MSG_CTRUNC) == 0);
	TEST_COMPARE (found_timestamp, 0);
      }
  }
}

static int
do_test (void)
{
  /* This test only make sense for ABIs that support 32 bit time_t socket
     timestampss.  */
  if (sizeof (time_t) > 4 && __TIMESIZE == 64)
    return 0;

  srv = xsocket (AF_INET, SOCK_DGRAM, 0);
  srv_addr = (struct sockaddr_in) {
    .sin_family = AF_INET,
    .sin_addr = {.s_addr = htonl (INADDR_LOOPBACK) },
  };
  xbind (srv, (struct sockaddr *) &srv_addr, sizeof (srv_addr));
  {
    socklen_t sa_len = sizeof (srv_addr);
    xgetsockname (srv, (struct sockaddr *) &srv_addr, &sa_len);
    TEST_VERIFY (sa_len == sizeof (srv_addr));
  }

  /* Check recvmsg;  */
  do_test_large_buffer (false);
  do_test_small_buffer (false);
  /* Check recvmmsg.  */
  do_test_large_buffer (true);
  do_test_small_buffer (true);

  return 0;
}

#include <support/test-driver.c>
