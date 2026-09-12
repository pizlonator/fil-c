/* Test for BZ #18287.
   This test verifies that gethostbyname_r correctly accounts for pointer
   alignment padding when calculating the remaining buffer size.
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

#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <support/check.h>
#include <support/check_nss.h>
#include <support/resolv_test.h>
#include <support/support.h>
#include <support/xmemstream.h>

static const char host_name[] = "foo.site.example";

/* Prepare big enough answer to trigger buffer overflow.  */
static void
response (const struct resolv_response_context *ctx,
          struct resolv_response_builder *b,
          const char *qname, uint16_t qclass, uint16_t qtype)
{
  if (strcmp (qname, host_name) == 0 && qtype == T_A)
    {
      struct resolv_response_flags flags = { };
      resolv_response_init (b, flags);
      resolv_response_add_question (b, qname, qclass, qtype);
      resolv_response_section (b, ns_s_an);

      for (int i = 0; i <= 60; i++)
        {
          char last_ch = (char) i;
          char addr_ipv4[4] = { 127, 126, 125, last_ch };
          resolv_response_open_record (b, qname, qclass, T_A, 0x12345678);
          resolv_response_add_data (b, addr_ipv4, sizeof (addr_ipv4));
          resolv_response_close_record (b);
        }
    }
}

/* Test gethostbyname_r with a specified buffer size and alignment.
   Returns true if the buffer is sufficient, false if it's too small.  */
static bool
query_host (size_t size, size_t align)
{
  struct hostent ret;
  struct hostent *result = NULL;
  int err;

  /* Allocate memory for buffer, alignment padding, and a 64-byte checking
     area.  */
  size_t total_alloc = size + align + 64;
  unsigned char *raw_buf = xmalloc (total_alloc);

  /* Fill the tail of the buffer with 0xAA to detect overflows below.  */
  memset (raw_buf, 0, size + align);
  memset (raw_buf + size + align, 0xAA, 64);

  char *ptr = (char *) raw_buf + align;

  int res = gethostbyname_r (host_name, &ret, ptr, size, &result, &err);

  /* Verify that the overflow guard region remains unchanged.  */
  for (int i = 0; i < 64; i++)
    if (raw_buf[size + align + i] != 0xAA)
      FAIL_EXIT1 ("Buffer overflow was detected! (align=%zu, size=%zu)",
                  align, size);

  bool is_sufficient = false;

  if (res == 0 && result != NULL)
    {
      /* Generate the expected response to satisfy check_hostent.  */
      struct xmemstream expected;
      xopen_memstream (&expected);

      fprintf (expected.out, "name: %s\n", host_name);
      for (int i = 0; i <= 60; i++)
        fprintf (expected.out, "address: 127.126.125.%d\n", i);

      xfclose_memstream (&expected);
      check_hostent (host_name, &ret, expected.buffer);
      free (expected.buffer);
      is_sufficient = true; /* Buffer is sufficient.  */
    }
  else if (res == ERANGE && err == NETDB_INTERNAL)
    is_sufficient = false; /* Buffer is too small.  */
  else
    FAIL_EXIT1 ("gethostbyname_r failed unexpectedly: res=%d, err=%d",
                res, err);

  free (raw_buf);
  return is_sufficient;
}

static int
do_test (void)
{
  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response,
     });

  int lower_bound = 512;
  int upper_bound = 2048;

  TEST_COMPARE (query_host (lower_bound, 0), false);
  TEST_COMPARE (query_host (upper_bound, 0), true);

  /* Finding the smallest hostent buffer size.  */
  while (upper_bound != lower_bound + 1)
    {
      int size = (lower_bound + upper_bound) / 2;
      if (query_host (size, 0))
        upper_bound = size;
      else
        lower_bound = size;
    }

  printf ("info: Boundary found. lower_bound=%d, upper_bound=%d\n",
          lower_bound, upper_bound);

  TEST_COMPARE (query_host (lower_bound, 0), false);
  TEST_COMPARE (query_host (upper_bound, 0), true);

  /* Trigger the vulnerability.
     Test all misalignments (1-7 bytes).  Note that it is expected that
     for certain alignments, the required buffer size increases.  This
     happens because gethostbyname_r applies internal padding to align
     pointers, which consumes available space.  Therefore, a patched glibc
     safely returns true (success) or false (ERANGE) depending on this
     padding.  A vulnerable glibc will fail to account for alignment
     padding, overflow the buffer, and cause a test failure.  */
  for (size_t align = 1; align < 8; align++)
    query_host (upper_bound, align);

  resolv_test_end (aux);

  return 0;
}

#include <support/test-driver.c>
