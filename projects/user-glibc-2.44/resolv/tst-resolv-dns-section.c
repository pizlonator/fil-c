/* Test handling of invalid section transitions (bug 34014).
   Copyright (C) 2022-2026 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/format_nss.h>
#include <support/resolv_test.h>
#include <support/support.h>

/* Name of test, and the second section type.  */
struct item {
  const char *test;
  int ns_section;
};

static const struct item test_items[] =
  {
    { "Test crossing from ns_s_an to ns_s_ar.", ns_s_ar },
    { "Test crossing from ns_s_an to ns_s_an.", ns_s_ns },

    { NULL, 0 },
  };

/* The response is designed to contain the following:
   - An Answer section with one T_PTR record that is skipped.
   - A second section with a semantically invalid T_PTR record.
   The original defect is that the response parsing would cross
   section boundaries and handle the additional section T_PTR
   as if it were an answer.  A conforming implementation would
   stop as soon as it reaches the end of the section.  */
static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  TEST_COMPARE (qclass, C_IN);

  /* We only test PTR.  */
  TEST_COMPARE (qtype, T_PTR);

  unsigned int count;
  char *tail = NULL;

  if (strstr (qname, "in-addr.arpa") != NULL
      && sscanf (qname, "%u.%ms", &count, &tail) == 2)
    TEST_COMPARE_STRING (tail, "0.168.192.in-addr.arpa");
  else if (sscanf (qname, "%x.%ms", &count, &tail) == 2)
    {
    TEST_COMPARE_STRING (tail, "\
0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa");
    }
  else
    FAIL_EXIT1 ("invalid QNAME: %s\n", qname);
  free (tail);

  /* We have a bounded number of possible tests.  */
  TEST_VERIFY (count >= 0);
  TEST_VERIFY (count <= 15);

  struct resolv_response_flags flags = {};
  resolv_response_init (b, flags);
  resolv_response_add_question (b, qname, qclass, qtype);
  resolv_response_section (b, ns_s_an);

  /* Actual answer record, but the wrong name (skipped).  */
  resolv_response_open_record (b, "1.0.0.10.in-addr.arpa", qclass, qtype, 60);

  /* Record the answer.  */
  resolv_response_add_name (b, "test.ptr.example.net");
  resolv_response_close_record (b);

  /* Add a second section to test section boundary crossing.  */
  resolv_response_section (b, test_items[count].ns_section);
  /* Semantically incorrect, but hide a T_PTR entry.  */
  resolv_response_open_record (b, qname, qclass, qtype, 60);
  resolv_response_add_name (b, "wrong.ptr.example.net");
  resolv_response_close_record (b);
}


/* Perform one check using a reverse lookup.  */
static void
check_reverse (int af, int count)
{
  TEST_VERIFY (af == AF_INET || af == AF_INET6);
  TEST_VERIFY (count < array_length (test_items));

  char addr[sizeof (struct in6_addr)] = { 0 };
  socklen_t addrlen;
  if (af == AF_INET)
    {
      addr[0] = (char) 192;
      addr[1] = (char) 168;
      addr[2] = (char) 0;
      addr[3] = (char) count;
      addrlen = 4;
    }
  else
    {
      addr[0] = 0x20;
      addr[1] = 0x01;
      addr[2] = 0x0d;
      addr[3] = 0xb8;
      addr[4] = addr[5] = addr[6] = addr[7] = 0x0;
      addr[8] = addr[9] = addr[10] = addr[11] = 0x0;
      addr[12] = 0x0;
      addr[13] = 0x0;
      addr[14] = 0x0;
      addr[15] = count;
      addrlen = 16;
    }

  h_errno = 0;
  struct hostent *answer = gethostbyaddr (addr, addrlen, af);
  TEST_VERIFY (answer == NULL);
  TEST_VERIFY (h_errno == NO_RECOVERY);
  if (answer != NULL)
    printf ("error: unexpected success: %s\n",
	    support_format_hostent (answer));
}

static int
do_test (void)
{
  struct resolv_test *obj = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response
     });

  for (int i = 0; test_items[i].test != NULL; i++)
    {
      check_reverse (AF_INET, i);
      check_reverse (AF_INET6, i);
    }

  resolv_test_end (obj);

  return 0;
}

#include <support/test-driver.c>
