/* Test for spurious timeouts with short 12-byte responses (bug 31890).
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

static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  switch (ctx->server_index)
    {
    case 0:
      /* First server times out.  */
      {
        struct resolv_response_flags flags = {.rcode = rcode};
        resolv_response_init (b, flags);
      }
      break;
    case 1:
      /* Second server sends reply.  */
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
      break;
    default:
      FAIL_EXIT1 ("unexpected query to server %d", ctx->server_index);
    }
}

static void
check_one (void)
{

  /* The buggy 1-second query timeout results in 30 seconds of delay,
     which triggers a test timeout failure.  */
  for (int i = 0;  i < 10; ++i)
    {
      check_hostent ("www.example", gethostbyname ("www.example"),
                     "name: www.example\n"
                     "address: 192.0.2.17\n");
      check_hostent ("www.example", gethostbyname2 ("www.example", AF_INET6),
                     "name: www.example\n"
                     "address: 2001:db8::1\n");
      static const struct addrinfo hints =
        {
          .ai_family = AF_UNSPEC,
          .ai_socktype = SOCK_STREAM,
        };
      struct addrinfo *ai;
      int ret = getaddrinfo ("www.example", "80", &hints, &ai);
      check_addrinfo ("www.example", ai, ret,
                      "address: STREAM/TCP 192.0.2.17 80\n"
                      "address: STREAM/TCP 2001:db8::1 80\n");
      if (ret == 0)
        freeaddrinfo (ai);
    }
}

static int
do_test (void)
{
  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response,
     });

  _res.options |= RES_SNGLKUP;

  rcode = 2; /* SERVFAIL.  */
  check_one ();

  rcode = 4; /* NOTIMP.  */
  check_one ();

  rcode = 5; /* REFUSED.  */
  check_one ();

  resolv_test_end (aux);

  return 0;
}

#include <support/test-driver.c>
