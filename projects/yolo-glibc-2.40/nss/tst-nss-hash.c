/* Test __nss_hash
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

#include <support/support.h>
#include <support/check.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <nss.h>
#include <simple-nss-hash.h>

uint32_t __nss_hash (const void *__key, size_t __length);

static int
do_fill_tests (size_t len, int fill)
{
  uint32_t expec, res;
  char buf[len];
  memset (buf, fill, len);

  expec = __simple_nss_hash (buf, len);
  res = __nss_hash (buf, len);
  if (expec != res)
    FAIL_EXIT1 ("FAIL: fill(%d) (%zu), %x != %x\n", fill, len, expec, res);

  return 0;
}

static int
do_rand_tests (size_t len)
{
  uint32_t expec, res;
  size_t i;
  char buf[len];
  for (i = 0; i < len; ++i)
    buf[i] = random ();

  expec = __simple_nss_hash (buf, len);
  res = __nss_hash (buf, len);
  if (expec != res)
    FAIL_EXIT1 ("FAIL: random (%zu), %x != %x\n", len, expec, res);

  return 0;
}

static int
do_test (void)
{
  size_t i, j;
  for (i = 0; i < 100; ++i)
    {
      for (j = 0; j < 8192; ++j)
	{
	  if (do_rand_tests (i))
	    return 1;

	  if (do_fill_tests (i, -1) || do_fill_tests (i, 1)
	      || do_fill_tests (i, 0x80) || do_fill_tests (i, 0x88))
	    return 1;
	}
    }
  return 0;
}

#include <support/test-driver.c>
