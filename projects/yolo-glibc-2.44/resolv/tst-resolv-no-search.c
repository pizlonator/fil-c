/* Test using "search ." in /etc/resolv.conf.
   Copyright (C) 2026 Free Software Foundation, Inc.
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
#include <support/namespace.h>
#include <support/resolv_test.h>
#include <support/support.h>

/* Used to check for duplicated queries (bug 33804).  POSIX does not
   explicitly say that socket calls (as used in the resolver tests)
   provide synchronization.  */
static _Atomic unsigned int query_count;

/* Check that plain res_init loads the configuration as expected.  */
static void
test_res_init (void *ignored)
{
  res_init ();
  TEST_COMPARE_STRING (_res.dnsrch[0], ".");
  TEST_COMPARE_STRING (_res.dnsrch[1], NULL);
}

static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  TEST_VERIFY_EXIT (qclass == C_IN);
  TEST_COMPARE (ctx->server_index, 0);
  ++query_count;

  if (strncmp (qname, "does-not-exist", strlen ("does-not-exist")) == 0)
    {
      resolv_response_init (b, (struct resolv_response_flags)
                            { .rcode = ns_r_nxdomain });
      resolv_response_add_question (b, qname, qclass, qtype);
      return;
    }

  resolv_response_init (b, (struct resolv_response_flags) { });
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
    }
  resolv_response_close_record (b);
}

static void
check_h (const char *name, int family, const char *expected)
{
  if (family == AF_INET)
    {
      char *query = xasprintf ("gethostbyname (\"%s\")", name);
      query_count = 0;
      check_hostent (query, gethostbyname (name), expected);
      TEST_COMPARE (query_count, 1);
      free (query);
    }
  {
    char *query = xasprintf ("gethostbyname2 (\"%s\", %d)", name, family);
    query_count = 0;
    check_hostent (query, gethostbyname2 (name, family), expected);
    TEST_COMPARE (query_count, 1);
    free (query);
  }
}

static void
check_ai (const char *name, int family, const char *expected)
{
  struct addrinfo hints = { .ai_family = family, .ai_socktype = SOCK_STREAM, };
  struct addrinfo *ai;
  char *query = xasprintf ("%s:80 [%d]", name, hints.ai_family);
  query_count = 0;
  int ret = getaddrinfo (name, "80", &hints, &ai);
  check_addrinfo (query, ai, ret, expected);
  TEST_COMPARE (query_count, family == AF_UNSPEC ? 2 : 1);
  if (ret == 0)
    freeaddrinfo (ai);
  free (query);
}

static int
do_test (void)
{
  support_isolate_in_subprocess (test_res_init, NULL);

  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response,
       .no_override_resolv_conf_search = true,
     });

  check_h ("www.example", AF_INET,
           "name: www.example\n"
           "address: 192.0.2.17\n");
  check_h ("www.example", AF_INET6,
           "name: www.example\n"
           "address: 2001:db8::1\n");
  check_ai ("www.example", AF_UNSPEC,
            "address: STREAM/TCP 192.0.2.17 80\n"
            "address: STREAM/TCP 2001:db8::1 80\n");
  check_ai ("www.example", AF_INET,
            "address: STREAM/TCP 192.0.2.17 80\n");
  check_ai ("www.example", AF_INET6,
            "address: STREAM/TCP 2001:db8::1 80\n");
  check_h ("does-not-exist.example", AF_INET,
           "error: HOST_NOT_FOUND\n");
  check_h ("does-not-exist.example", AF_INET6,
           "error: HOST_NOT_FOUND\n");
  check_ai ("does-not-exist.example", AF_UNSPEC,
            "error: Name or service not known\n");
  check_ai ("does-not-exist.example", AF_INET,
            "error: Name or service not known\n");
  check_ai ("does-not-exist.example", AF_INET6,
            "error: Name or service not known\n");

  /* With trailing dot.  */
  check_h ("www.example.", AF_INET,
           "name: www.example\n"
           "address: 192.0.2.17\n");
  check_h ("www.example.", AF_INET6,
           "name: www.example\n"
           "address: 2001:db8::1\n");
  check_ai ("www.example.", AF_UNSPEC,
            "address: STREAM/TCP 192.0.2.17 80\n"
            "address: STREAM/TCP 2001:db8::1 80\n");
  check_ai ("www.example.", AF_INET,
            "address: STREAM/TCP 192.0.2.17 80\n");
  check_ai ("www.example.", AF_INET6,
            "address: STREAM/TCP 2001:db8::1 80\n");
  check_h ("does-not-exist.example.", AF_INET,
           "error: HOST_NOT_FOUND\n");
  check_h ("does-not-exist.example.", AF_INET6,
           "error: HOST_NOT_FOUND\n");
  check_ai ("does-not-exist.example.", AF_UNSPEC,
            "error: Name or service not known\n");
  check_ai ("does-not-exist.example.", AF_INET,
            "error: Name or service not known\n");
  check_ai ("does-not-exist.example.", AF_INET6,
            "error: Name or service not known\n");

  resolv_test_end (aux);

  return 0;
}

#include <support/test-driver.c>
