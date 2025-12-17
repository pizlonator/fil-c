/* Test of <sys/socket.h> substitute.
   Copyright (C) 2007, 2009-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2007.  */

#include <config.h>

#include <sys/socket.h>

/* POSIX mandates that AF_UNSPEC shall be 0.  */
static_assert (AF_UNSPEC == 0);

/* Check that the 'socklen_t' type is defined.  */
socklen_t t1;

/* Check that the 'size_t' and 'ssize_t' types are defined.  */
size_t t2;
ssize_t t3;

/* Check that 'struct iovec' is defined.  */
struct iovec io;

/* Check that a minimal set of 'struct msghdr' is defined.  */
struct msghdr msg;

#include <errno.h>

#include "intprops.h"

/* POSIX requires that 'socklen_t' is an integer type with a width of at
   least 32 bits.  */
static_assert (32 <= TYPE_WIDTH (socklen_t));

/* POSIX requires that sa_family_t is an unsigned integer type.  */
static_assert (! TYPE_SIGNED (sa_family_t));


int
main (void)
{
  struct sockaddr_storage x;
  sa_family_t i;

  /* Check some errno values.  */
  switch (ENOTSOCK)
    {
    case ENOTSOCK:
    case EADDRINUSE:
    case ENETRESET:
    case ECONNABORTED:
    case ECONNRESET:
    case ENOTCONN:
    case ESHUTDOWN:
      break;
    }

  /* Check that each supported address family has a distinct value.  */
  switch (0)
    {
    case AF_UNSPEC:
#if HAVE_IPV4
    case AF_INET:
#endif
#if HAVE_IPV6
    case AF_INET6:
#endif
#if HAVE_UNIXSOCKET
    case AF_UNIX:
#endif
    default:
      break;
    }

  /* Check that the shutdown type macros are defined to distinct values.  */
#if HAVE_SHUTDOWN
  switch (0)
    {
    case SHUT_RD:
    case SHUT_WR:
    case SHUT_RDWR:
    default:
      break;
    }
#endif

  x.ss_family = 42;
  i = 42;
  msg.msg_iov = &io;

  return (x.ss_family - i + msg.msg_namelen + msg.msg_iov->iov_len
          + msg.msg_iovlen);
}
