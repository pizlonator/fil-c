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

#include <array_length.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <support/check.h>
#include <support/next_to_fault.h>
#include <support/support.h>
#include <support/test-driver.h>
#include <support/xunistd.h>
#include <support/xsocket.h>
#include <sys/mman.h>

/* Some extra space added for ancillary data, it might be used to convert
   32-bit timestamp to 64-bit for _TIME_BITS=64.  */
enum { slack_max_size = 64 };
static const int slack[] = { 0, 4, 8, 16, 32, slack_max_size };

static bool support_64_timestamp;
/* AF_INET socket and address used to receive data.  */
static int srv;
static struct sockaddr_in srv_addr;

static int
do_sendto (const struct sockaddr_in *addr, int nmsgs)
{
  int s = xsocket (AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  xconnect (s, (const struct sockaddr *) addr, sizeof (*addr));

  for (int i = 0; i < nmsgs; i++)
    xsendto (s, &i, sizeof (i), 0, (const struct sockaddr *) addr,
	     sizeof (*addr));

  xclose (s);

  return 0;
}

static void
do_recvmsg_slack_ancillary (bool use_multi_call, int s, void *cmsg,
			    size_t slack, size_t tsize, int exp_payload)
{
  int payload;
  struct iovec iov =
    {
      .iov_base = &payload,
      .iov_len = sizeof (payload)
    };
  size_t msg_controllen = CMSG_SPACE (tsize) + slack;
  char *msg_control = cmsg - msg_controllen;
  memset (msg_control, 0x55, msg_controllen);
  struct mmsghdr mmhdr =
    {
      .msg_hdr =
      {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = msg_control,
        .msg_controllen = msg_controllen
      },
    };

  int r;
  if (use_multi_call)
    {
      r = recvmmsg (s, &mmhdr, 1, 0, NULL);
      if (r >= 0)
	r = mmhdr.msg_len;
    }
  else
    r = recvmsg (s, &mmhdr.msg_hdr, 0);
  TEST_COMPARE (r, sizeof (int));
  TEST_COMPARE (payload, exp_payload);

  if (cmsg == NULL)
    return;

  /* A timestamp is expected if 32-bit timestamp are used (support in every
     configuration) or if underlying kernel support 64-bit timestamps.
     Otherwise recvmsg will need extra space do add the 64-bit timestamp.  */
  bool exp_timestamp;
  if (sizeof (time_t) == 4 || support_64_timestamp)
    exp_timestamp = true;
   else
    exp_timestamp = slack >= CMSG_SPACE (tsize);

  bool timestamp = false;
  for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&mmhdr.msg_hdr);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR (&mmhdr.msg_hdr, cmsg))
    {
      if (cmsg->cmsg_level != SOL_SOCKET)
	continue;
      if (cmsg->cmsg_type == SCM_TIMESTAMP
	  && cmsg->cmsg_len == CMSG_LEN (sizeof (struct timeval)))
	{
	  struct timeval tv;
	  memcpy (&tv, CMSG_DATA (cmsg), sizeof (tv));
	  if (test_verbose)
	    printf ("SCM_TIMESTAMP:   {%jd, %jd}\n", (intmax_t)tv.tv_sec,
		    (intmax_t)tv.tv_usec);
	  timestamp = true;
	}
      else if (cmsg->cmsg_type == SCM_TIMESTAMPNS
	       && cmsg->cmsg_len == CMSG_LEN (sizeof (struct timespec)))
	{
	  struct timespec ts;
	  memcpy (&ts, CMSG_DATA (cmsg), sizeof (ts));
	  if (test_verbose)
	    printf ("SCM_TIMESTAMPNS: {%jd, %jd}\n", (intmax_t)ts.tv_sec,
		    (intmax_t)ts.tv_nsec);
	  timestamp = true;
	}
    }

  TEST_COMPARE (timestamp, exp_timestamp);
}

/* Check if the extra ancillary space is correctly handled by recvmsg and
   recvmmsg with different extra space for the ancillaty buffer.  */
static void
do_test_slack_space (void)
{
  /* Setup the ancillary data buffer with an extra page with PROT_NONE to
     check the possible timestamp conversion on some systems.  */
  struct support_next_to_fault nf =
    support_next_to_fault_allocate (slack_max_size);
  void *msgbuf = nf.buffer + slack_max_size;

  /* Enable the timestamp using struct timeval precision.  */
  {
    int r = setsockopt (srv, SOL_SOCKET, SO_TIMESTAMP, &(int){1},
			sizeof (int));
    TEST_VERIFY_EXIT (r != -1);
  }
  /* Check recvmsg.  */
  do_sendto (&srv_addr, array_length (slack));
  for (int s = 0; s < array_length (slack); s++)
    {
      memset (nf.buffer, 0x55, nf.length);
      do_recvmsg_slack_ancillary (false, srv, msgbuf, slack[s],
				  sizeof (struct timeval), s);
    }
  /* Check recvmmsg.  */
  do_sendto (&srv_addr, array_length (slack));
  for (int s = 0; s < array_length (slack); s++)
    {
      memset (nf.buffer, 0x55, nf.length);
      do_recvmsg_slack_ancillary (true, srv, msgbuf, slack[s],
				  sizeof (struct timeval), s);
    }

  /* Now enable timestamp using a higher precision, it overwrites the previous
     precision.  */
  {
    int r = setsockopt (srv, SOL_SOCKET, SO_TIMESTAMPNS, &(int){1},
			sizeof (int));
    TEST_VERIFY_EXIT (r != -1);
  }
  /* Check recvmsg.  */
  do_sendto (&srv_addr, array_length (slack));
  for (int s = 0; s < array_length (slack); s++)
    do_recvmsg_slack_ancillary (false, srv, msgbuf, slack[s],
				sizeof (struct timespec), s);
  /* Check recvmmsg.  */
  do_sendto (&srv_addr, array_length (slack));
  for (int s = 0; s < array_length (slack); s++)
    do_recvmsg_slack_ancillary (true, srv, msgbuf, slack[s],
				sizeof (struct timespec), s);

  support_next_to_fault_free (&nf);
}

/* Check if the converted 64-bit timestamp is correctly appended when there
   are multiple ancillary messages.  */
static void
do_recvmsg_multiple_ancillary (bool use_multi_call, int s, void *cmsg,
			       size_t cmsgsize, int exp_msg)
{
  int msg;
  struct iovec iov =
    {
      .iov_base = &msg,
      .iov_len = sizeof (msg)
    };
  size_t msgs = cmsgsize;
  struct mmsghdr mmhdr =
    {
      .msg_hdr =
      {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_controllen = msgs,
        .msg_control = cmsg,
      },
    };

  int r;
  if (use_multi_call)
    {
      r = recvmmsg (s, &mmhdr, 1, 0, NULL);
      if (r >= 0)
	r = mmhdr.msg_len;
    }
  else
    r = recvmsg (s, &mmhdr.msg_hdr, 0);
  TEST_COMPARE (r, sizeof (int));
  TEST_COMPARE (msg, exp_msg);

  if (cmsg == NULL)
    return;

  bool timestamp = false;
  bool origdstaddr = false;
  for (struct cmsghdr *cmsg = CMSG_FIRSTHDR (&mmhdr.msg_hdr);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR (&mmhdr.msg_hdr, cmsg))
    {
      if (cmsg->cmsg_level == SOL_IP
	  && cmsg->cmsg_type == IP_ORIGDSTADDR
	  && cmsg->cmsg_len >= CMSG_LEN (sizeof (struct sockaddr_in)))
	{
	  struct sockaddr_in sa;
	  memcpy (&sa, CMSG_DATA (cmsg), sizeof (sa));
	  if (test_verbose)
	    {
	      char str[INET_ADDRSTRLEN];
	      inet_ntop (AF_INET, &sa.sin_addr, str, INET_ADDRSTRLEN);
	      printf ("IP_ORIGDSTADDR:  %s:%d\n", str, ntohs (sa.sin_port));
	    }
	  origdstaddr = sa.sin_addr.s_addr == srv_addr.sin_addr.s_addr
			&& sa.sin_port == srv_addr.sin_port;
	}
      if (cmsg->cmsg_level == SOL_SOCKET
	  && cmsg->cmsg_type == SCM_TIMESTAMP
	  && cmsg->cmsg_len >= CMSG_LEN (sizeof (struct timeval)))
	{
	  struct timeval tv;
	  memcpy (&tv, CMSG_DATA (cmsg), sizeof (tv));
	  if (test_verbose)
	    printf ("SCM_TIMESTAMP:   {%jd, %jd}\n", (intmax_t)tv.tv_sec,
		    (intmax_t)tv.tv_usec);
	  timestamp = true;
	}
    }

  TEST_COMPARE (timestamp, true);
  TEST_COMPARE (origdstaddr, true);
}

static void
do_test_multiple_ancillary (void)
{
  {
    int r = setsockopt (srv, SOL_SOCKET, SO_TIMESTAMP, &(int){1},
			sizeof (int));
    TEST_VERIFY_EXIT (r != -1);
  }
  {
    int r = setsockopt (srv, IPPROTO_IP, IP_RECVORIGDSTADDR, &(int){1},
			sizeof (int));
    TEST_VERIFY_EXIT (r != -1);
  }

  /* Enough data for default SO_TIMESTAMP, the IP_RECVORIGDSTADDR, and the
     extra 64-bit SO_TIMESTAMP.  */
  enum { msgbuflen = CMSG_SPACE (2 * sizeof (uint64_t))
		     + CMSG_SPACE (sizeof (struct sockaddr_in))
		     + CMSG_SPACE (2 * sizeof (uint64_t)) };
  char msgbuf[msgbuflen];

  enum { nmsgs = 8 };
  /* Check recvmsg.  */
  do_sendto (&srv_addr, nmsgs);
  for (int s = 0; s < nmsgs; s++)
    do_recvmsg_multiple_ancillary (false, srv, msgbuf, msgbuflen, s);
  /* Check recvmmsg.  */
  do_sendto (&srv_addr, nmsgs);
  for (int s = 0; s < nmsgs; s++)
    do_recvmsg_multiple_ancillary (true, srv, msgbuf, msgbuflen, s);
}

static int
do_test (void)
{
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

  TEST_COMPARE (recvmsg (-1, NULL, 0), -1);
  TEST_COMPARE (errno, EBADF);
  TEST_COMPARE (recvmmsg (-1, NULL, 0, 0, NULL), -1);
  TEST_COMPARE (errno, EBADF);

  /* If underlying kernel does not support   */
  support_64_timestamp = support_socket_so_timestamp_time64 (srv);

  do_test_slack_space ();
  do_test_multiple_ancillary ();

  xclose (srv);

  return 0;
}

#include <support/test-driver.c>
