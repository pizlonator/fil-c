/* Test that malloc tcache catches double free.
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

#include <errno.h>
#include <error.h>
#include <limits.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>

/* Test for a double free where the size information gets overwritten by a
 * terminating null byte.  */
static int
do_test (void)
{
  /* The payload is exactly 0x19 Bytes long:
   * 0x18 bytes 'B' and one terminating null byte
   */
  const char *payload = "BBBBBBBBBBBBBBBBBBBBBBBB";

  char *volatile first_chunk
      = malloc (strlen (payload)); // <-- off by one error
  char *volatile second_chunk = malloc (0x118);

  // free the second chunk the first time now it is in the tcache with tc_idx =
  free (second_chunk);

  // change the the size of the second_chunk using the terminating null byte if
  // the PAYLOAD
  strcpy (first_chunk, payload);

  // now the second_chunk has a new size
  // calling free a second time will not trigger the double free detection
  free (second_chunk);

  printf ("FAIL: tcache double free not detected\n");
  return 1;
}

#define TEST_FUNCTION do_test
#define EXPECTED_SIGNAL SIGABRT
#include <support/test-driver.c>
