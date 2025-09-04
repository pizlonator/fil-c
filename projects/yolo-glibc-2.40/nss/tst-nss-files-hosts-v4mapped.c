/* Test lookup of IPv4 mapped addresses in files database (bug 25457)

   Copyright (C) 2023-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>

static int
do_test (void)
{
  struct sockaddr_in6 s6 = { .sin6_family = AF_INET6 };
  inet_pton (AF_INET6, "::ffff:192.168.0.1", &s6.sin6_addr);

  char node[NI_MAXHOST];
  int res = getnameinfo ((struct sockaddr *) &s6, sizeof (s6), node,
			 sizeof (node), NULL, 0, NI_NAMEREQD);
  if (res)
    printf ("%d %s\n", res, gai_strerror (res));
  else
    printf ("node=%s\n", node);

  return res != 0;
}

#include <support/test-driver.c>
