/* Test for BZ #15218.  Verify that getaddrinfo returns FQDN in
   ai_canonname, when AI_CANONNAME is requested and search domain apply.
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

static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  if (strcmp (qname, "foo.site.example") == 0
      || strcmp (qname, "bar.foo.site.example") == 0
      || strcmp (qname, "site.example") == 0)
    {
      struct resolv_response_flags flags = { };
      resolv_response_init (b, flags);
      resolv_response_add_question (b, qname, qclass, qtype);
      resolv_response_section (b, ns_s_an);
      if (qtype == T_A)
        {
          char addr_ipv4[4] = { 127, 126, 125, 124 };
          resolv_response_open_record (b, qname, qclass, T_A, 0x12345678);
          resolv_response_add_data (b, addr_ipv4, sizeof (addr_ipv4));
          resolv_response_close_record (b);
        }
      else if (qtype == T_AAAA)
        {
          char addr_ipv6[16] =
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
          resolv_response_open_record (b, qname, qclass, T_AAAA, 0x12345678);
          resolv_response_add_data (b, addr_ipv6, sizeof (addr_ipv6));
          resolv_response_close_record (b);
        }
      else
        FAIL_EXIT1 ("qtype must be one of A, AAAA");
    }
  else
    {
      struct resolv_response_flags flags = {.rcode = ns_r_nxdomain, };
      resolv_response_init (b, flags);
      resolv_response_add_question (b, qname, qclass, qtype);
    }
}

void
query_host (const char *host_name, const char *expected_name)
{
  int family[] = { AF_INET, AF_INET6, AF_UNSPEC };
  const char *family_names[] = { "AF_INET", "AF_INET6", "AF_UNSPEC" };

  for (int i = 0; i < 3; i++)
    {
      struct addrinfo hints = {
        .ai_socktype = 0,
        .ai_protocol = 0,
        .ai_family = family[i],
        .ai_flags = AI_CANONNAME,
      };
      struct addrinfo *result, *current;
      int res = getaddrinfo (host_name, NULL, &hints, &result);
      if (res != 0)
        FAIL_EXIT1 ("getaddrinfo (%s, %s): %s\n", host_name, family_names[i],
                    gai_strerror (res));
      else
        {
          int count = 0;
          for (current = result;
               current != NULL && current->ai_canonname != NULL;
               current = current->ai_next)
            {
              TEST_COMPARE_STRING (current->ai_canonname, expected_name);
              count++;
            }
          freeaddrinfo (result);
          if (count > 1)
            FAIL_EXIT1 ("Expected exactly one canonname, but got %d\n", count);
        }
    }
}

/* test with site.example domain.  */
void
test_search_with_site_example_domain (void)
{
  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response,
       .search = { "site.example" },
     });

  query_host ("foo", "foo.site.example");
  query_host ("bar.foo", "bar.foo.site.example");

  resolv_test_end (aux);
}

/* test with example domain.  */
void
test_search_with_example_domain (void)
{
  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response,
       .search = { "example" },
     });

  query_host ("foo.site", "foo.site.example");
  query_host ("bar.foo.site", "bar.foo.site.example");
  query_host ("site", "site.example");

  resolv_test_end (aux);
}

static int
do_test (void)
{
  test_search_with_site_example_domain ();
  test_search_with_example_domain ();

  return 0;
}

#include <support/test-driver.c>
