/* Test for BZ #14308.
   Verify that getaddrinfo (AF_UNSPEC) succeeds if one of the A/AAAA
   responses is a NOERROR/NODATA reply with recursion unavailable (RA=0),
   but the other response contains a usable address.

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
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/check_nss.h>
#include <support/resolv_test.h>
#include <support/support.h>

enum scenario
{
  /* A contains data, AAAA is NOERROR/NODATA with RA cleared.  */
  A_AUTH_AAAA_FAKE,

  /* AAAA contains data, A is NOERROR/NODATA with RA cleared.  */
  A_FAKE_AAAA_AUTH
};

static enum scenario current_scenario;

/* Create a NOERROR reply without answers and with RA cleared.  */
static void
add_fake (struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  struct resolv_response_flags flags =
    {
      .rcode = ns_r_noerror,
      .clear_ra = true
    };
  resolv_response_init (b, flags);
  resolv_response_add_question (b, qname, qclass, qtype);
}

static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  if (strcmp (qname, "foo.site.example") != 0)
    FAIL_EXIT1 ("Unexpected qname: %s", qname);

  if (qtype == T_A)
    {
      if (current_scenario == A_AUTH_AAAA_FAKE)
        {
          struct resolv_response_flags flags = { .rcode = ns_r_noerror };
          resolv_response_init (b, flags);
          resolv_response_add_question (b, qname, qclass, qtype);

          resolv_response_section (b, ns_s_an);
          resolv_response_open_record (b, qname, qclass, T_A, 100);
          char addr_ipv4[4] = { 127, 128, 129, 130 };
          resolv_response_add_data (b, addr_ipv4, sizeof (addr_ipv4));
          resolv_response_close_record (b);
        }
      else if (current_scenario == A_FAKE_AAAA_AUTH)
        add_fake (b, qname, qclass, qtype);
      else
        FAIL_EXIT1 ("Unknown scenario: %d", current_scenario);

      return;
    }

  if (qtype == T_AAAA)
    {
      if (current_scenario == A_AUTH_AAAA_FAKE)
        add_fake (b, qname, qclass, qtype);
      else if (current_scenario == A_FAKE_AAAA_AUTH)
        {
          struct resolv_response_flags flags = { .rcode = ns_r_noerror };
          resolv_response_init (b, flags);
          resolv_response_add_question (b, qname, qclass, qtype);

          resolv_response_section (b, ns_s_an);
          resolv_response_open_record (b, qname, qclass, T_AAAA, 100);
          char addr_ipv6[16] =
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
          resolv_response_add_data (b, addr_ipv6, sizeof (addr_ipv6));
          resolv_response_close_record (b);
        }
      else
        FAIL_EXIT1 ("Unknown scenario: %d", current_scenario);

      return;
    }

  FAIL_EXIT1 ("qtype must be one of A, AAAA");
}

static void
query_host (const char *host_name)
{
  struct addrinfo hints =
    {
      .ai_socktype = SOCK_STREAM,
      .ai_family = AF_UNSPEC,
    };

  struct addrinfo *result = NULL;
  int res = getaddrinfo (host_name, "80", &hints, &result);

  if (current_scenario == A_AUTH_AAAA_FAKE)
    check_addrinfo (host_name, result, res,
                    "address: STREAM/TCP 127.128.129.130 80\n");
  else if (current_scenario == A_FAKE_AAAA_AUTH)
    check_addrinfo (host_name, result, res,
                    "address: STREAM/TCP ::1 80\n");
  else
    FAIL_EXIT1 ("Unexpected scenario: %d", current_scenario);

  if (res == 0)
    freeaddrinfo (result);
}

static int
do_test (void)
{
  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response,
     });

  current_scenario = A_AUTH_AAAA_FAKE;
  query_host ("foo.site.example");

  current_scenario = A_FAKE_AAAA_AUTH;
  query_host ("foo.site.example");

  resolv_test_end (aux);
  return 0;
}

#include <support/test-driver.c>
