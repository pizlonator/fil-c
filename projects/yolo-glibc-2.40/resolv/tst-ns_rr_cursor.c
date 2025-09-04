/* Tests for resource record parsing.
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

#include <arpa/nameser.h>
#include <string.h>
#include <support/check.h>
#include <support/next_to_fault.h>

/* Reference packet for packet parsing.  */
static const unsigned char valid_packet[] =
  { 0x11, 0x12, 0x13, 0x14,
    0x00, 0x01,               /* Question count.  */
    0x00, 0x02,               /* Answer count.  */
    0x21, 0x22, 0x23, 0x24,   /* Other counts (not actually in packet).  */
    3, 'w', 'w', 'w', 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0,
    0x00, 0x1c,               /* Question type: AAAA.  */
    0x00, 0x01,               /* Question class: IN.  */
    0xc0, 0x0c,               /* Compression reference to QNAME.  */
    0x00, 0x1c,               /* Record type: AAAA.  */
    0x00, 0x01,               /* Record class: IN.  */
    0x12, 0x34, 0x56, 0x78,   /* Record TTL.  */
    0x00, 0x10,               /* Record data length (16 bytes).  */
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, /* IPv6 address.  */
    0xc0, 0x0c,               /* Compression reference to QNAME.  */
    0x00, 0x1c,               /* Record type: AAAA.  */
    0x00, 0x01,               /* Record class: IN.  */
    0x11, 0x33, 0x55, 0x77,   /* Record TTL.  */
    0x00, 0x10,               /* Record data length (16 bytes).  */
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, /* IPv6 address.  */
  };

/* Special offsets in valid_packet.  */
enum
  {
    offset_of_first_record = 29,
    offset_of_second_record = 57,
  };

/* Check that parsing valid_packet succeeds.  */
static void
test_valid (void)
{
  struct ns_rr_cursor c;
  TEST_VERIFY_EXIT (__ns_rr_cursor_init (&c, valid_packet,
                                         sizeof (valid_packet)));
  TEST_COMPARE (ns_rr_cursor_rcode (&c), 4);
  TEST_COMPARE (ns_rr_cursor_ancount (&c), 2);
  TEST_COMPARE (ns_rr_cursor_nscount (&c), 0x2122);
  TEST_COMPARE (ns_rr_cursor_adcount (&c), 0x2324);
  TEST_COMPARE_BLOB (ns_rr_cursor_qname (&c), 13, &valid_packet[12], 13);
  TEST_COMPARE (ns_rr_cursor_qtype (&c), T_AAAA);
  TEST_COMPARE (ns_rr_cursor_qclass (&c), C_IN);
  TEST_COMPARE (c.current - valid_packet, offset_of_first_record);

  struct ns_rr_wire r;
  TEST_VERIFY_EXIT (__ns_rr_cursor_next (&c, &r));
  TEST_COMPARE (r.rtype, T_AAAA);
  TEST_COMPARE (r.rclass, C_IN);
  TEST_COMPARE (r.ttl, 0x12345678);
  TEST_COMPARE_BLOB (r.rdata, r.rdlength,
                     "\x90\x91\x92\x93\x94\x95\x96\x97"
                     "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f", 16);
  TEST_COMPARE (c.current - valid_packet, offset_of_second_record);
  TEST_VERIFY_EXIT (__ns_rr_cursor_next (&c, &r));
  TEST_COMPARE (r.rtype, T_AAAA);
  TEST_COMPARE (r.rclass, C_IN);
  TEST_COMPARE (r.ttl, 0x11335577);
  TEST_COMPARE_BLOB (r.rdata, r.rdlength,
                     "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7"
                     "\xa8\xa9\xaa\xab\xac\xad\xae\xaf", 16);
  TEST_VERIFY (c.current == c.end);
}

/* Check that trying to parse a packet with a compressed QNAME fails.  */
static void
test_compressed_qname (void)
{
  static const unsigned char packet[] =
    { 0x11, 0x12, 0x13, 0x14,
      0x00, 0x01,               /* Question count.  */
      0x00, 0x00,               /* Answer count.  */
      0x00, 0x00, 0x00, 0x00,   /* Other counts.  */
      3, 'w', 'w', 'w', 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0xc0, 0x04,
      0x00, 0x01,               /* Question type: A.  */
      0x00, 0x01,               /* Question class: IN.  */
    };

  struct ns_rr_cursor c;
  TEST_VERIFY_EXIT (!__ns_rr_cursor_init (&c, packet, sizeof (packet)));
}

/* Check that trying to parse a packet with two questions fails.  */
static void
test_two_questions (void)
{
  static const unsigned char packet[] =
    { 0x11, 0x12, 0x13, 0x14,
      0x00, 0x02,               /* Question count.  */
      0x00, 0x00,               /* Answer count.  */
      0x00, 0x00, 0x00, 0x00,   /* Other counts.  */
      3, 'w', 'w', 'w', 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0xc0, 0x04,
      0x00, 0x01,               /* Question type: A.  */
      0x00, 0x01,               /* Question class: IN.  */
      3, 'w', 'w', 'w', 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0xc0, 0x04,
      0x00, 0x1c,               /* Question type: AAAA.  */
      0x00, 0x01,               /* Question class: IN.  */
    };

  struct ns_rr_cursor c;
  TEST_VERIFY_EXIT (!__ns_rr_cursor_init (&c, packet, sizeof (packet)));
}

/* Used to check that parsing truncated packets does not over-read.  */
static struct support_next_to_fault ntf;

/* Truncated packet in the second resource record.  */
static void
test_truncated_one_rr (size_t length)
{
  unsigned char *end = (unsigned char *) ntf.buffer - ntf.length;
  unsigned char *start = end - length;

  /* Produce the truncated packet.  */
  memcpy (start, valid_packet, length);

  struct ns_rr_cursor c;
  TEST_VERIFY_EXIT (__ns_rr_cursor_init (&c, start, length));
  TEST_COMPARE (ns_rr_cursor_rcode (&c), 4);
  TEST_COMPARE (ns_rr_cursor_ancount (&c), 2);
  TEST_COMPARE (ns_rr_cursor_nscount (&c), 0x2122);
  TEST_COMPARE (ns_rr_cursor_adcount (&c), 0x2324);
  TEST_COMPARE_BLOB (ns_rr_cursor_qname (&c), 13, &valid_packet[12], 13);
  TEST_COMPARE (ns_rr_cursor_qtype (&c), T_AAAA);
  TEST_COMPARE (ns_rr_cursor_qclass (&c), C_IN);
  TEST_COMPARE (c.current - start, offset_of_first_record);

  struct ns_rr_wire r;
  TEST_VERIFY_EXIT (__ns_rr_cursor_next (&c, &r));
  TEST_COMPARE (r.rtype, T_AAAA);
  TEST_COMPARE (r.rclass, C_IN);
  TEST_COMPARE (r.ttl, 0x12345678);
  TEST_COMPARE_BLOB (r.rdata, r.rdlength,
                     "\x90\x91\x92\x93\x94\x95\x96\x97"
                     "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f", 16);
  TEST_COMPARE (c.current - start, offset_of_second_record);
  TEST_VERIFY (!__ns_rr_cursor_next (&c, &r));
}

/* Truncated packet in the first resource record.  */
static void
test_truncated_no_rr (size_t length)
{
  unsigned char *end = (unsigned char *) ntf.buffer - ntf.length;
  unsigned char *start = end - length;

  /* Produce the truncated packet.  */
  memcpy (start, valid_packet, length);

  struct ns_rr_cursor c;
  TEST_VERIFY_EXIT (__ns_rr_cursor_init (&c, start, length));
  TEST_COMPARE (ns_rr_cursor_rcode (&c), 4);
  TEST_COMPARE (ns_rr_cursor_ancount (&c), 2);
  TEST_COMPARE (ns_rr_cursor_nscount (&c), 0x2122);
  TEST_COMPARE (ns_rr_cursor_adcount (&c), 0x2324);
  TEST_COMPARE_BLOB (ns_rr_cursor_qname (&c), 13, &valid_packet[12], 13);
  TEST_COMPARE (ns_rr_cursor_qtype (&c), T_AAAA);
  TEST_COMPARE (ns_rr_cursor_qclass (&c), C_IN);
  TEST_COMPARE (c.current - start, offset_of_first_record);

  struct ns_rr_wire r;
  TEST_VERIFY (!__ns_rr_cursor_next (&c, &r));
}

/* Truncated packet before first resource record.  */
static void
test_truncated_before_rr (size_t length)
{
  unsigned char *end = (unsigned char *) ntf.buffer - ntf.length;
  unsigned char *start = end - length;

  /* Produce the truncated packet.  */
  memcpy (start, valid_packet, length);

  struct ns_rr_cursor c;
  TEST_VERIFY_EXIT (!__ns_rr_cursor_init (&c, start, length));
}

static int
do_test (void)
{
  ntf = support_next_to_fault_allocate (sizeof (valid_packet));

  test_valid ();
  test_compressed_qname ();
  test_two_questions ();

  for (int length = offset_of_second_record; length < sizeof (valid_packet);
       ++length)
    test_truncated_one_rr (length);
  for (int length = offset_of_first_record; length < offset_of_second_record;
       ++length)
    test_truncated_no_rr (length);
  for (int length = 0; length < offset_of_first_record; ++length)
    test_truncated_before_rr (length);

  support_next_to_fault_free (&ntf);
  return 0;
}

#include <support/test-driver.c>
