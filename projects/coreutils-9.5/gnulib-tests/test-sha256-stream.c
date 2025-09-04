/*
 * Copyright (C) 2018-2024 Free Software Foundation, Inc.
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

#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

#define TESTFILE "test-sha256.data"
#include "test-digest.h"

int
main (void)
{
  /* Test sha224_stream.  */
  test_digest_on_files (sha224_stream, "sha224_stream", 28,
                        "\xd1\x4a\x02\x8c\x2a\x3a\x2b\xc9\x47\x61\x02\xbb\x28\x82\x34\xc4\x15\xa2\xb0\x1f\x82\x8e\xa6\x2a\xc5\xb3\xe4\x2f",
                        "\xe8\x87\x99\xb0\xd0\xd5\xbe\xcc\x67\x91\x83\x7f\xa9\x53\x88\xd4\x05\x6f\x12\x50\xa5\x11\xd1\x48\x29\x76\x66\x63",
                        "\x86\xa2\x5a\xc3\x6a\x70\x8e\xf3\x4e\x78\x55\xf0\xb1\x97\x8a\xa8\x61\x78\x6c\xb8\x87\xcd\x5c\xf1\x19\x3b\x53\xe4");

  /* Test sha256_stream.  */
  test_digest_on_files (sha256_stream, "sha256_stream", 32,
                        "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24\x27\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52\xb8\x55",
                        "\xb4\x7c\xc0\xf1\x04\xb6\x2d\x4c\x7c\x30\xbc\xd6\x8f\xd8\xe6\x76\x13\xe2\x87\xdc\x4a\xd8\xc3\x10\xef\x10\xcb\xad\xea\x9c\x43\x80",
                        "\xe5\x19\x95\xff\x0b\xc4\x95\xcf\x49\xdd\x31\x42\x0a\x6b\xe0\x6b\x38\xef\xef\x43\xab\xa6\xf4\x76\x64\x32\x99\x4a\x47\x1a\x0f\xb5");

  return 0;
}
