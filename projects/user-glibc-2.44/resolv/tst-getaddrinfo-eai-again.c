/* Test for BZ #16849. Verify that getaddrinfo correctly returns
   EAI_AGAIN error code if DNS query fails due to a network failure.

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <netdb.h>
#include <support/check.h>
#include <support/namespace.h>

/* Query getaddrinfo with various address families and verify that
   it returns EAI_AGAIN when DNS resolution fails.  */
static void
query_host (const char *host_name)
{
  int family[] = { AF_INET, AF_INET6, AF_UNSPEC };
  const char *family_names[] = { "AF_INET", "AF_INET6", "AF_UNSPEC" };

  for (int i = 0; i < 3; i++)
    {
      struct addrinfo hints =
        {
          .ai_socktype = 0,
          .ai_protocol = 0,
          .ai_family = family[i],
          .ai_flags = 0,
        };
      struct addrinfo *result;
      int res = getaddrinfo (host_name, NULL, &hints, &result);
      if (res != EAI_AGAIN)
        FAIL_EXIT1 ("getaddrinfo (%s, %s) returned %s, expected EAI_AGAIN",
                    host_name, family_names[i], gai_strerror (res));
    }
}

static int
do_test (void)
{
  /* Hide any name server that might be running on 127.0.0.1.  */
  support_enter_network_namespace ();

  query_host ("site.example");
  return 0;
}

#include <support/test-driver.c>
