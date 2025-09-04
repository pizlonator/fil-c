/*
 * Copyright (C) 2017-2024 Free Software Foundation, Inc.
 * Written by Jia Zhang <qianyue.zj@alibaba-inc.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include "sm3.h"

#include <stdio.h>
#include <string.h>

static int
test (const char *in, const char *out)
{
  char buf[SM3_DIGEST_SIZE];

  if (memcmp (sm3_buffer (in, strlen (in), buf),
              out, SM3_DIGEST_SIZE) != 0)
    {
      size_t i;
      printf ("expected:\n");
      for (i = 0; i < SM3_DIGEST_SIZE; i++)
        printf ("%02x ", out[i] & 0xFFu);
      printf ("\ncomputed:\n");
      for (i = 0; i < SM3_DIGEST_SIZE; i++)
        printf ("%02x ", buf[i] & 0xFFu);
      printf ("\n");
      return 1;
    }

  return 0;
}

int
main (void)
{
  /* Test vectors from GM/T 004-2012 */
  const char *in[] =
    {
      "abc",
      "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
    };
  const char *out[] =
    {
      "\x66\xc7\xf0\xf4\x62\xee\xed\xd9\xd1\xf2\xd4\x6b\xdc\x10\xe4\xe2"
      "\x41\x67\xc4\x87\x5c\xf2\xf7\xa2\x29\x7d\xa0\x2b\x8f\x4b\xa8\xe0",
      "\xde\xbe\x9f\xf9\x22\x75\xb8\xa1\x38\x60\x48\x89\xc1\x8e\x5a\x4d"
      "\x6f\xdb\x70\xe5\x38\x7e\x57\x65\x29\x3d\xcb\xa3\x9c\x0c\x57\x32",
    };
  size_t i;

  for (i = 0; i < sizeof (in) / sizeof (in[0]); i++)
    {
      if (test (in[i], out[i]))
        return 1;
    }

  return 0;
}
