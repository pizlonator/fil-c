/*
 * Copyright (C) 2005, 2009-2024 Free Software Foundation, Inc.
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

/* Written by Simon Josefsson. */

#include <config.h>

#include "md5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "macros.h"

#define TESTFILE "test-md5.data"
#include "test-digest.h"

int
main (void)
{
  /* Test md5_stream.  */
  test_digest_on_files (md5_stream, "md5_stream", 16,
                        "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e",
                        "\x0d\x70\x06\xcd\x05\x5e\x94\xcf\x61\x45\x87\xe1\xd2\xae\x0c\x8e",
                        "\xec\x99\x67\x9b\xff\xc0\xf9\xb0\x6d\x18\x30\x6b\x06\xd6\x56\x23");

  return 0;
}
