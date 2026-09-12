/* Test for BZ #16849. Verify that getaddrinfo correctly returns
   EAI_AGAIN when DNS resolution fails due to timeout or malformed
   responses.

   This test uses two simulated failure modes:
   - The DNS server does not respond at all (resolv_response_drop).
   - The DNS server responds with a zero-length packet.

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
#include <resolv.h>
#include <stdbool.h>
#include <support/check.h>
#include <support/resolv_test.h>

/* Track whether the callbacks were actually invoked.  */
static volatile bool response_called_drop = false;
static volatile bool response_called_zero_len = false;

/* Simulate a DNS server that sends a zero-length response.  */
static void
response_zero_len (const struct resolv_response_context *ctx,
                   struct resolv_response_builder *b,
                   const char *qname, uint16_t qclass, uint16_t qtype)
{
  response_called_zero_len = true;
  /* Do nothing — zero-length reply.  */
}

/* Simulate a DNS server that drops the request.  */
static void
response_drop (const struct resolv_response_context *ctx,
               struct resolv_response_builder *b,
               const char *qname, uint16_t qclass, uint16_t qtype)
{
  response_called_drop = true;
  resolv_response_drop (b);
}

/* Query getaddrinfo for multiple families and expect EAI_AGAIN.  */
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

/* Simulate DNS server dropping all queries.  */
static void
test_drop (void)
{
  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response_drop,
     });
  /* Reduce default timeout to make the test run faster.  */
  _res.retrans = 1;
  _res.retry = 1;
  query_host ("site.example");
  resolv_test_end (aux);
}

/* Simulate DNS server sending zero-length responses.  */
static void
test_zero_len_packet (void)
{
  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response_zero_len,
     });
  query_host ("site.example");
  resolv_test_end (aux);
}

static int
do_test (void)
{
  test_drop ();
  test_zero_len_packet ();

  if (!response_called_drop)
    FAIL_EXIT1 ("response_drop callback was not called");
  if (!response_called_zero_len)
    FAIL_EXIT1 ("response_zero_len callback was not called");
  return 0;
}

#include <support/test-driver.c>
