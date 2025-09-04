/* Test parallel failure/success responses (bug 30081).
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#include <resolv.h>
#include <support/check.h>
#include <support/resolv_test.h>
#include <support/check_nss.h>

/* The rcode in the initial response.  */
static volatile int rcode;

/* Whether to fail the initial A query (!fail_aaaa) or the initial
   AAAA query (fail_aaaa).  */
static volatile bool fail_aaaa;

static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  /* Handle the failing query.  */
  if ((fail_aaaa && qtype == T_AAAA) && ctx->server_index == 0)
    {
      struct resolv_response_flags flags = {.rcode = rcode};
      resolv_response_init (b, flags);
      return;
    }

  /* Otherwise produce a response.  */
  resolv_response_init (b, (struct resolv_response_flags) {});
  resolv_response_add_question (b, qname, qclass, qtype);
  resolv_response_section (b, ns_s_an);
  resolv_response_open_record (b, qname, qclass, qtype, 0);
  switch (qtype)
    {
    case T_A:
      {
        char ipv4[4] = {192, 0, 2, 17};
        resolv_response_add_data (b, &ipv4, sizeof (ipv4));
      }
      break;
    case T_AAAA:
      {
        char ipv6[16]
          = {0x20, 0x01, 0xd, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        resolv_response_add_data (b, &ipv6, sizeof (ipv6));
      }
      break;
    default:
      FAIL_EXIT1 ("unexpected TYPE%d query", qtype);
    }
  resolv_response_close_record (b);
}

static void
check_one (void)
{

  /* The buggy 1-second query timeout results in 30 seconds of delay,
     which triggers are test timeout failure.  */
  for (int i = 0;  i < 30; ++i)
    {
      static const struct addrinfo hints =
        {
          .ai_family = AF_UNSPEC,
          .ai_socktype = SOCK_STREAM,
        };
      struct addrinfo *ai;
      int ret = getaddrinfo ("www.example", "80", &hints, &ai);
      const char *expected;
      if (ret == 0 && ai->ai_next != NULL)
        expected = ("address: STREAM/TCP 192.0.2.17 80\n"
                    "address: STREAM/TCP 2001:db8::1 80\n");
      else
        /* Only one response because the AAAA lookup failure is
           treated as an ignoreable error.  */
        expected = "address: STREAM/TCP 192.0.2.17 80\n";
      check_addrinfo ("www.example", ai, ret, expected);
      if (ret == 0)
        freeaddrinfo (ai);
    }
}

static int
do_test (void)
{
  for (int do_single_lookup = 0; do_single_lookup < 2; ++do_single_lookup)
    {
      struct resolv_test *aux = resolv_test_start
        ((struct resolv_redirect_config)
         {
           .response_callback = response,
         });

      if (do_single_lookup)
        _res.options |= RES_SNGLKUP;

      for (int do_fail_aaaa = 0; do_fail_aaaa < 2; ++do_fail_aaaa)
        {
          fail_aaaa = do_fail_aaaa;

          rcode = 2; /* SERVFAIL.  */
          check_one ();

          rcode = 4; /* NOTIMP.  */
          check_one ();

          rcode = 5; /* REFUSED.  */
          check_one ();
        }

      resolv_test_end (aux);
    }

  return 0;
}

#include <support/test-driver.c>
