/*
 * Copyright (C) 2005, 2008-2024 Free Software Foundation, Inc.
 * Written by Simon Josefsson
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

#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

#define TESTFILE "test-sha1.data"
#include "test-digest.h"

int
main (void)
{
  /* Test sha1_stream.  */
  test_digest_on_files (sha1_stream, "sha1_stream", 20,
                        "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55\xbf\xef\x95\x60\x18\x90\xaf\xd8\x07\x09",
                        "\x9c\x04\xcd\x63\x72\x07\x7e\x9b\x11\xf7\x0c\xa1\x11\xc9\x80\x7d\xc7\x13\x7e\x4b",
                        "\x91\xab\x6b\x1b\x8d\x29\x25\x3c\xcb\x8d\xce\xb7\x7a\x25\x26\x2c\x92\xc9\x22\x09");

  return 0;
}
