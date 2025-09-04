/* Test the RES_NOAAAA resolver option with a large response.
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

#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdbool.h>
#include <stdlib.h>
#include <support/check.h>
#include <support/check_nss.h>
#include <support/resolv_test.h>
#include <support/support.h>
#include <support/xmemstream.h>

/* Used to keep track of the number of queries.  */
static volatile unsigned int queries;

/* If true, add a large TXT record at the start of the answer section.  */
static volatile bool stuff_txt;

static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  /* If not using TCP, just force its use.  */
  if (!ctx->tcp)
    {
      struct resolv_response_flags flags = {.tc = true};
      resolv_response_init (b, flags);
      resolv_response_add_question (b, qname, qclass, qtype);
      return;
    }

  /* The test needs to send four queries, the first three are used to
     grow the NSS buffer via the ERANGE handshake.  */
  ++queries;
  TEST_VERIFY (queries <= 4);

  /* AAAA queries are supposed to be disabled.  */
  TEST_COMPARE (qtype, T_A);
  TEST_COMPARE (qclass, C_IN);
  TEST_COMPARE_STRING (qname, "example.com");

  struct resolv_response_flags flags = {};
  resolv_response_init (b, flags);
  resolv_response_add_question (b, qname, qclass, qtype);

  resolv_response_section (b, ns_s_an);

  if (stuff_txt)
    {
      resolv_response_open_record (b, qname, qclass, T_TXT, 60);
      int zero = 0;
      for (int i = 0; i <= 15000; ++i)
        resolv_response_add_data (b, &zero, sizeof (zero));
      resolv_response_close_record (b);
    }

  for (int i = 0; i < 200; ++i)
    {
      resolv_response_open_record (b, qname, qclass, qtype, 60);
      char ipv4[4] = {192, 0, 2, i + 1};
      resolv_response_add_data (b, &ipv4, sizeof (ipv4));
      resolv_response_close_record (b);
    }
}

static int
do_test (void)
{
  struct resolv_test *obj = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response
     });

  _res.options |= RES_NOAAAA;

  for (int do_stuff_txt = 0; do_stuff_txt < 2; ++do_stuff_txt)
    {
      queries = 0;
      stuff_txt = do_stuff_txt;

      struct addrinfo *ai = NULL;
      int ret;
      ret = getaddrinfo ("example.com", "80",
                         &(struct addrinfo)
                         {
                           .ai_family = AF_UNSPEC,
                           .ai_socktype = SOCK_STREAM,
                         }, &ai);

      char *expected_result;
      {
        struct xmemstream mem;
        xopen_memstream (&mem);
        for (int i = 0; i < 200; ++i)
          fprintf (mem.out, "address: STREAM/TCP 192.0.2.%d 80\n", i + 1);
        xfclose_memstream (&mem);
        expected_result = mem.buffer;
      }

      check_addrinfo ("example.com", ai, ret, expected_result);

      free (expected_result);
      freeaddrinfo (ai);
    }

  resolv_test_end (obj);
  return 0;
}

#include <support/test-driver.c>
