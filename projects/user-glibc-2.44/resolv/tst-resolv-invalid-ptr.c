/* Test handling of invalid T_PTR results (bug 34015).
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

/* Name of test, the answer, the expected error return, and if we
   expect the call to fail.  */
struct item {
  const char *test;
  const char *answer;
  int expected;
  bool fail;
};

static const struct item test_items[] =
  {
    /* Test for invalid characters.  */
    { "Invalid use of \"|\"",
      "test.|.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"&\"",
      "test.&.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \";\"",
      "test.;.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"<\"",
      "test.<.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \">\"",
      "test.>.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"(\"",
      "test.(.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \")\"",
      "test.).ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"$\"",
      "test.$.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"`\"",
      "test.`.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"\\\"",
      "test.\\.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"\'\"",
      "test.'.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"\"\"",
      "test.\".ptr.example", NO_RECOVERY, true },
    { "Invalid use of \" \"",
      "test. .ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"\\t\"",
      "test.\t.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"\\n\"",
      "test.\n.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"\\r\"",
      "test.\r.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"*\"",
      "test.*.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"?\"",
      "test.?.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"[\"",
      "test.[.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"]\"",
      "test.].ptr.example", NO_RECOVERY, true },
    { "Invalid use of \",\"",
      "test.,.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"~\"",
      "test.~.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \":\"",
      "test.:.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"!\"",
      "test.!.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"@\"",
      "test.@.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"#\"",
      "test.#.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"%\"",
      "test.%%.ptr.example", NO_RECOVERY, true },
    { "Invalid use of \"^\"",
      "test.^.ptr.example", NO_RECOVERY, true },

    /* Test for invalid UTF-8 characters (2-byte, 4-byte, 6-byte).  */
    { "Invalid use of UTF-8 (2-byte, U+00C0-U+00C2)",
      "ÁÂÃ.test.ptr.example", NO_RECOVERY, true },
    { "Invalid use of UTF-8 (4-byte, U+0750-U+0752)",
      "ݐݑݒ.test.ptr.example", NO_RECOVERY, true },
     { "Invalid use of UTF-8 (6-byte, U+0904-U+0906)",
      "ऄअआ.test.ptr.example", NO_RECOVERY, true },

    /* Test for "-" which may be valid depending on position.  */
    { "Invalid leading \"-\"",
      "-test.ptr.example", NO_RECOVERY, true },
    { "Valid trailing \"-\"",
      "test-.ptr.example", 0, false },
    { "Valid mid-label use of \"-\"",
      "te-st.ptr.example", 0, false },

    /* Test for "_" which is always valid in any position.  */
    { "Valid leading use of \"_\"",
      "_test.ptr.example", 0, false },
    { "Valid mid-label use of \"_\"",
      "te_st.ptr.example", 0, false },
    { "Valid trailing use of \"_\"",
      "test_.ptr.example", 0, false },

    /* Sanity test the broader set [A-Za-z0-9_-] of valid characters.  */
    { "Valid \"[A-Z]\"",
      "test.ABCDEFGHIJKLMNOPQRSTUVWXYZ.ptr.example", 0, false },
    { "Valid \"[a-z]\"",
      "test.abcdefghijklmnopqrstuvwxyz.ptr.example", 0, false },
    { "Valid \"[0-9]\"",
      "test.0123456789.ptr.example", 0, false },
    { "Valid mixed use of \"[A-Za-z0-9_-]\"",
      "test.012abcABZ_-.ptr.example", 0, false },
  };

static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  TEST_COMPARE (qclass, C_IN);

  /* We only test PTR.  */
  TEST_COMPARE (qtype, T_PTR);

  unsigned int count, count1;
  char *tail = NULL;

  /* The test implementation can handle up to 255 tests.  */
  if (strstr (qname, "in-addr.arpa") != NULL
      && sscanf (qname, "%u.%ms", &count, &tail) == 2)
    TEST_COMPARE_STRING (tail, "0.168.192.in-addr.arpa");
  else if (sscanf (qname, "%x.%x.%ms", &count, &count1, &tail) == 3)
    {
      TEST_COMPARE_STRING (tail, "\
0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa");
      count |= count1 << 4;
    }
  else
    FAIL_EXIT1 ("invalid QNAME: %s\n", qname);
  free (tail);

  /* Cross check. Count has a fixed bound (soft limit).  */
  TEST_VERIFY (count >= 0 && count <= 255);

  /* We have a fixed number of tests (hard limit).  */
  TEST_VERIFY_EXIT (count < array_length (test_items));

  struct resolv_response_flags flags = {};
  resolv_response_init (b, flags);
  resolv_response_add_question (b, qname, qclass, qtype);
  resolv_response_section (b, ns_s_an);

  /* Actual answer record.  */
  resolv_response_open_record (b, qname, qclass, qtype, 60);

  /* Record the answer.  */
  resolv_response_add_name (b, test_items[count].answer);
  resolv_response_close_record (b);
}

/* Perform one check using a reverse lookup.  */
static void
check_reverse (int af, int count)
{
  TEST_VERIFY (af == AF_INET || af == AF_INET6);
  TEST_VERIFY_EXIT (count < array_length (test_items));

  /* Generate an address to query for each test.  */
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
      addr[15] = (char) count;
      addrlen = 16;
    }

  h_errno = 0;
  struct hostent *answer = gethostbyaddr (addr, addrlen, af);

  /* Verify h_errno is as expected.  */
  TEST_COMPARE (h_errno, test_items[count].expected);
  if (h_errno != test_items[count].expected)
    /* And print more information if it's not.  */
    printf ("INFO: %s\n", test_items[count].test);

  if (test_items[count].fail)
    {
      /* We expected a failure so verify answer is NULL.  */
      TEST_VERIFY (answer == NULL);
      /* If it's not NULL we should print out what we received.  */
      if (answer != NULL)
        printf ("error: unexpected success: %s\n",
		support_format_hostent (answer));
    }
  else
    /* We don't expect a failure so answer must be valid.  */
    TEST_COMPARE_STRING (answer->h_name, test_items[count].answer);
}

static int
do_test (void)
{
  struct resolv_test *obj = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response
     });

  for (int i = 0; i < array_length (test_items); i++)
    {
      check_reverse (AF_INET, i);
      check_reverse (AF_INET6, i);
    }
  resolv_test_end (obj);

  return 0;
}

#include <support/test-driver.c>
