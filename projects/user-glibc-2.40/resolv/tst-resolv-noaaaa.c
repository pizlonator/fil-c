/* Test the RES_NOAAAA resolver option.
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
#include <stdlib.h>
#include <support/check.h>
#include <support/check_nss.h>
#include <support/resolv_test.h>
#include <support/support.h>

/* Used to keep track of the number of queries.  */
static volatile unsigned int queries;

static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  /* Each test should only send one query.  */
  ++queries;
  TEST_COMPARE (queries, 1);

  /* AAAA queries are supposed to be disabled.  */
  TEST_VERIFY (qtype != T_AAAA);
  TEST_COMPARE (qclass, C_IN);

  /* The only other query type besides A is PTR.  */
  if (qtype != T_A)
    TEST_COMPARE (qtype, T_PTR);

  int an, ns, ar;
  char *tail;
  if (sscanf (qname, "an%d.ns%d.ar%d.%ms", &an, &ns, &ar, &tail) != 4)
    FAIL_EXIT1 ("invalid QNAME: %s\n", qname);
  TEST_COMPARE_STRING (tail, "example");
  free (tail);

  if (an < 0 || ns < 0 || ar < 0)
    {
      struct resolv_response_flags flags = { .rcode = NXDOMAIN, };
      resolv_response_init (b, flags);
      resolv_response_add_question (b, qname, qclass, qtype);
      return;
    }

  struct resolv_response_flags flags = {};
  resolv_response_init (b, flags);
  resolv_response_add_question (b, qname, qclass, qtype);

  resolv_response_section (b, ns_s_an);
  for (int i = 0; i < an; ++i)
    {
      resolv_response_open_record (b, qname, qclass, qtype, 60);
      switch (qtype)
        {
        case T_A:
          {
            char ipv4[4] = {192, 0, 2, i + 1};
            resolv_response_add_data (b, &ipv4, sizeof (ipv4));
          }
          break;

        case T_PTR:
          {
            char *name = xasprintf ("ptr-%d", i);
            resolv_response_add_name (b, name);
            free (name);
          }
          break;
        }
      resolv_response_close_record (b);
    }

  resolv_response_section (b, ns_s_ns);
  for (int i = 0; i < ns; ++i)
    {
      resolv_response_open_record (b, qname, qclass, T_NS, 60);
      char *name = xasprintf ("ns%d.example.net", i);
      resolv_response_add_name (b, name);
      free (name);
      resolv_response_close_record (b);
    }

  resolv_response_section (b, ns_s_ar);
  int addr = 1;
  for (int i = 0; i < ns; ++i)
    {
      char *name = xasprintf ("ns%d.example.net", i);
      for (int j = 0; j < ar; ++j)
        {
          resolv_response_open_record (b, name, qclass, T_A, 60);
          char ipv4[4] = {192, 0, 2, addr};
          resolv_response_add_data (b, &ipv4, sizeof (ipv4));
          resolv_response_close_record (b);

          resolv_response_open_record (b, name, qclass, T_AAAA, 60);
          char ipv6[16]
            = {0x20, 0x01, 0xd, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, addr};
          resolv_response_add_data (b, &ipv6, sizeof (ipv6));
          resolv_response_close_record (b);

          ++addr;
        }
      free (name);
    }
}

/* Number of modes.  Lowest bit encodes *n* function vs implicit _res
   argument.  The mode numbers themselves are arbitrary.  */
enum { mode_count = 8 };

/* res_send-like modes do not perform error translation.  */
enum { first_send_mode = 6 };

static int
libresolv_query (unsigned int mode, const char *qname, uint16_t qtype,
                 unsigned char *buf, size_t buflen)
{
  int saved_errno = errno;

  TEST_VERIFY_EXIT (mode < mode_count);

  switch (mode)
    {
    case 0:
      return res_query (qname, C_IN, qtype, buf, buflen);
    case 1:
      return res_nquery (&_res, qname, C_IN, qtype, buf, buflen);
    case 2:
      return res_search (qname, C_IN, qtype, buf, buflen);
    case 3:
      return res_nsearch (&_res, qname, C_IN, qtype, buf, buflen);
    case 4:
      return res_querydomain (qname, "", C_IN, qtype, buf, buflen);
    case 5:
      return res_nquerydomain (&_res, qname, "", C_IN, qtype, buf, buflen);
    case 6:
      {
        unsigned char querybuf[512];
        int ret = res_mkquery (QUERY, qname, C_IN, qtype,
                               NULL, 0, NULL, querybuf, sizeof (querybuf));
        TEST_VERIFY_EXIT (ret > 0);
        errno = saved_errno;
        return res_send (querybuf, ret, buf, buflen);
      }
    case 7:
      {
        unsigned char querybuf[512];
        int ret = res_nmkquery (&_res, QUERY, qname, C_IN, qtype,
                                NULL, 0, NULL, querybuf, sizeof (querybuf));
        TEST_VERIFY_EXIT (ret > 0);
        errno = saved_errno;
        return res_nsend (&_res, querybuf, ret, buf, buflen);
      }
    }
  __builtin_unreachable ();
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

  check_hostent ("an1.ns2.ar1.example",
                 gethostbyname ("an1.ns2.ar1.example"),
                 "name: an1.ns2.ar1.example\n"
                 "address: 192.0.2.1\n");
  queries = 0;
  check_hostent ("an0.ns2.ar1.example",
                 gethostbyname ("an0.ns2.ar1.example"),
                 "error: NO_ADDRESS\n");
  queries = 0;
  check_hostent ("an-1.ns2.ar1.example",
                 gethostbyname ("an-1.ns2.ar1.example"),
                 "error: HOST_NOT_FOUND\n");
  queries = 0;

  check_hostent ("an1.ns2.ar1.example AF_INET",
                 gethostbyname2 ("an1.ns2.ar1.example", AF_INET),
                 "name: an1.ns2.ar1.example\n"
                 "address: 192.0.2.1\n");
  queries = 0;
  check_hostent ("an0.ns2.ar1.example AF_INET",
                 gethostbyname2 ("an0.ns2.ar1.example", AF_INET),
                 "error: NO_ADDRESS\n");
  queries = 0;
  check_hostent ("an-1.ns2.ar1.example AF_INET",
                 gethostbyname2 ("an-1.ns2.ar1.example", AF_INET),
                 "error: HOST_NOT_FOUND\n");
  queries = 0;

  check_hostent ("an1.ns2.ar1.example AF_INET6",
                 gethostbyname2 ("an1.ns2.ar1.example", AF_INET6),
                 "error: NO_ADDRESS\n");
  queries = 0;
  check_hostent ("an0.ns2.ar1.example AF_INET6",
                 gethostbyname2 ("an0.ns2.ar1.example", AF_INET6),
                 "error: NO_ADDRESS\n");
  queries = 0;
  check_hostent ("an-1.ns2.ar1.example AF_INET6",
                 gethostbyname2 ("an-1.ns2.ar1.example", AF_INET6),
                 "error: HOST_NOT_FOUND\n");
  queries = 0;

  /* Multiple addresses.  */
  check_hostent ("an2.ns0.ar0.example",
                 gethostbyname ("an2.ns0.ar0.example"),
                 "name: an2.ns0.ar0.example\n"
                 "address: 192.0.2.1\n"
                 "address: 192.0.2.2\n");
  queries = 0;
  check_hostent ("an2.ns0.ar0.example AF_INET6",
                 gethostbyname2 ("an2.ns0.ar0.example", AF_INET6),
                 "error: NO_ADDRESS\n");
  queries = 0;

  /* getaddrinfo checks with one address.  */
  struct addrinfo *ai;
  int ret;
  ret = getaddrinfo ("an1.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_INET,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an1.ns2.ar1.example (AF_INET)", ai, ret,
                  "address: STREAM/TCP 192.0.2.1 80\n");
  freeaddrinfo (ai);
  queries = 0;
  ret = getaddrinfo ("an1.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_INET6,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an1.ns2.ar1.example (AF_INET6)", ai, ret,
                  "error: No address associated with hostname\n");
  queries = 0;
  ret = getaddrinfo ("an1.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_UNSPEC,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an1.ns2.ar1.example (AF_UNSPEC)", ai, ret,
                  "address: STREAM/TCP 192.0.2.1 80\n");
  freeaddrinfo (ai);
  queries = 0;

  /* getaddrinfo checks with three addresses.  */
  ret = getaddrinfo ("an3.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_INET,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an3.ns2.ar1.example (AF_INET)", ai, ret,
                  "address: STREAM/TCP 192.0.2.1 80\n"
                  "address: STREAM/TCP 192.0.2.2 80\n"
                  "address: STREAM/TCP 192.0.2.3 80\n");
  freeaddrinfo (ai);
  queries = 0;
  ret = getaddrinfo ("an3.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_INET6,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an3.ns2.ar1.example (AF_INET6)", ai, ret,
                  "error: No address associated with hostname\n");
  queries = 0;
  ret = getaddrinfo ("an3.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_UNSPEC,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an3.ns2.ar1.example (AF_UNSPEC)", ai, ret,
                  "address: STREAM/TCP 192.0.2.1 80\n"
                  "address: STREAM/TCP 192.0.2.2 80\n"
                  "address: STREAM/TCP 192.0.2.3 80\n");
  freeaddrinfo (ai);
  queries = 0;

  /* getaddrinfo checks with no address.  */
  ret = getaddrinfo ("an0.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_INET,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an0.ns2.ar1.example (AF_INET)", ai, ret,
                  "error: No address associated with hostname\n");
  queries = 0;
  ret = getaddrinfo ("an0.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_INET6,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an0.ns2.ar1.example (AF_INET6)", ai, ret,
                  "error: No address associated with hostname\n");
  queries = 0;
  ret = getaddrinfo ("an0.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_UNSPEC,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an-1.ns2.ar1.example (AF_UNSPEC)", ai, ret,
                  "error: No address associated with hostname\n");
  queries = 0;

  /* getaddrinfo checks with NXDOMAIN.  */
  ret = getaddrinfo ("an-1.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_INET,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an-1.ns2.ar1.example (AF_INET)", ai, ret,
                  "error: Name or service not known\n");
  queries = 0;
  ret = getaddrinfo ("an-1.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_INET6,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an-1.ns2.ar1.example (AF_INET6)", ai, ret,
                  "error: Name or service not known\n");
  queries = 0;
  ret = getaddrinfo ("an-1.ns2.ar1.example", "80",
                     &(struct addrinfo)
                     {
                       .ai_family = AF_UNSPEC,
                       .ai_socktype = SOCK_STREAM,
                     }, &ai);
  check_addrinfo ("an-1.ns2.ar1.example (AF_UNSPEC)", ai, ret,
                  "error: Name or service not known\n");
  queries = 0;

  for (unsigned int mode = 0; mode < mode_count; ++mode)
    {
      unsigned char *buf;
      int ret;

      /* Response for A.  */
      buf = malloc (512);
      ret = libresolv_query (mode, "an1.ns2.ar1.example", T_A, buf, 512);
      TEST_VERIFY_EXIT (ret > 0);
      check_dns_packet ("an1.ns2.ar1.example A", buf, ret,
                        "name: an1.ns2.ar1.example\n"
                        "address: 192.0.2.1\n");
      free (buf);
      queries = 0;

      /* NODATA response for A.  */
      buf = malloc (512);
      errno = 0;
      ret = libresolv_query (mode, "an0.ns2.ar1.example", T_A, buf, 512);
      if (mode < first_send_mode)
        {
          TEST_COMPARE (ret, -1);
          TEST_COMPARE (errno, 0);
          TEST_COMPARE (h_errno, NO_ADDRESS);
        }
      else
        {
          TEST_VERIFY_EXIT (ret > 0);
          TEST_COMPARE (((HEADER *)buf)->rcode, 0);
          check_dns_packet ("an1.ns2.ar1.example A", buf, ret,
                            "name: an0.ns2.ar1.example\n");
        }
      free (buf);
      queries = 0;

      /* NXDOMAIN response for A.  */
      buf = malloc (512);
      errno = 0;
      ret = libresolv_query (mode, "an-1.ns2.ar1.example", T_A, buf, 512);
      if (mode < first_send_mode)
        {
          TEST_COMPARE (ret, -1);
          TEST_COMPARE (errno, 0);
          TEST_COMPARE (h_errno, HOST_NOT_FOUND);
        }
      else
        {
          TEST_VERIFY_EXIT (ret > 0);
          TEST_COMPARE (((HEADER *)buf)->rcode, NXDOMAIN);
          check_dns_packet ("an1.ns2.ar1.example A", buf, ret,
                            "name: an-1.ns2.ar1.example\n");
        }
      free (buf);
      queries = 0;

      /* Response for PTR.  */
      buf = malloc (512);
      ret = libresolv_query (mode, "an1.ns2.ar1.example", T_PTR, buf, 512);
      TEST_VERIFY_EXIT (ret > 0);
      check_dns_packet ("an1.ns2.ar1.example PTR", buf, ret,
                        "name: an1.ns2.ar1.example\n"
                        "data: an1.ns2.ar1.example PTR ptr-0\n");
      free (buf);
      queries = 0;

      /* NODATA response for PTR.  */
      buf = malloc (512);
      errno = 0;
      ret = libresolv_query (mode, "an0.ns2.ar1.example", T_PTR, buf, 512);
      if (mode < first_send_mode)
        {
          TEST_COMPARE (ret, -1);
          TEST_COMPARE (errno, 0);
          TEST_COMPARE (h_errno, NO_ADDRESS);
        }
      else
        {
          TEST_VERIFY_EXIT (ret > 0);
          TEST_COMPARE (((HEADER *)buf)->rcode, 0);
          check_dns_packet ("an1.ns2.ar1.example PTR", buf, ret,
                            "name: an0.ns2.ar1.example\n");
        }
      free (buf);
      queries = 0;

      /* NXDOMAIN response for PTR.  */
      buf = malloc (512);
      errno = 0;
      ret = libresolv_query (mode, "an-1.ns2.ar1.example", T_PTR, buf, 512);
      if (mode < first_send_mode)
        {
          TEST_COMPARE (ret, -1);
          TEST_COMPARE (errno, 0);
          TEST_COMPARE (h_errno, HOST_NOT_FOUND);
        }
      else
        {
          TEST_VERIFY_EXIT (ret > 0);
          TEST_COMPARE (((HEADER *)buf)->rcode, NXDOMAIN);
          check_dns_packet ("an1.ns2.ar1.example PTR", buf, ret,
                            "name: an-1.ns2.ar1.example\n");
        }
      free (buf);
      queries = 0;

      /* NODATA response for AAAA.  */
      buf = malloc (512);
      errno = 0;
      ret = libresolv_query (mode, "an1.ns2.ar1.example", T_AAAA, buf, 512);
      if (mode < first_send_mode)
        {
          TEST_COMPARE (ret, -1);
          TEST_COMPARE (errno, 0);
          TEST_COMPARE (h_errno, NO_ADDRESS);
        }
      else
        {
          TEST_VERIFY_EXIT (ret > 0);
          TEST_COMPARE (((HEADER *)buf)->rcode, 0);
          check_dns_packet ("an1.ns2.ar1.example A", buf, ret,
                            "name: an1.ns2.ar1.example\n");
        }
      free (buf);
      queries = 0;

      /* NODATA response for AAAA (original is already NODATA).  */
      buf = malloc (512);
      errno = 0;
      ret = libresolv_query (mode, "an0.ns2.ar1.example", T_AAAA, buf, 512);
      if (mode < first_send_mode)
        {
          TEST_COMPARE (ret, -1);
          TEST_COMPARE (errno, 0);
          TEST_COMPARE (h_errno, NO_ADDRESS);
        }
      else
        {
          TEST_VERIFY_EXIT (ret > 0);
          TEST_COMPARE (((HEADER *)buf)->rcode, 0);
          check_dns_packet ("an0.ns2.ar1.example A", buf, ret,
                            "name: an0.ns2.ar1.example\n");
        }
      free (buf);
      queries = 0;

      /* NXDOMAIN response.  */
      buf = malloc (512);
      errno = 0;
      ret = libresolv_query (mode, "an-1.ns2.ar1.example", T_AAAA, buf, 512);
      if (mode < first_send_mode)
        {
          TEST_COMPARE (ret, -1);
          TEST_COMPARE (errno, 0);
          TEST_COMPARE (h_errno, HOST_NOT_FOUND);
        }
      else
        {
          TEST_VERIFY_EXIT (ret > 0);
          TEST_COMPARE (((HEADER *)buf)->rcode, NXDOMAIN);
          check_dns_packet ("an-1.ns2.ar1.example A", buf, ret,
                            "name: an-1.ns2.ar1.example\n");
        }
      free (buf);
      queries = 0;
    }

  resolv_test_end (obj);

  return 0;
}

#include <support/test-driver.c>
