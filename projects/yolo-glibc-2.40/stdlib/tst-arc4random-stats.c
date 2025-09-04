/* Statistical tests for arc4random-related functions.
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

#include <array_length.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <support/check.h>

enum
{
  arc4random_key_size = 32
};

struct key
{
  unsigned char data[arc4random_key_size];
};

/* With 12,000 keys, the probability that a byte in a predetermined
   position does not have a predetermined value in all generated keys
   is about 4e-21.  The probability that this happens with any of the
   16 * 256 possible byte position/values is 1.6e-17.  This results in
   an acceptably low false-positive rate.  */
enum { key_count = 12000 };

static struct key keys[key_count];

/* Used to perform the distribution check.  */
static int byte_counts[arc4random_key_size][256];

/* Bail out after this many failures.  */
enum { failure_limit = 100 };

static void
find_stuck_bytes (bool (*func) (unsigned char *key))
{
  memset (&keys, 0xcc, sizeof (keys));

  int failures = 0;
  for (int key = 0; key < key_count; ++key)
    {
      while (true)
        {
          if (func (keys[key].data))
            break;
          ++failures;
          if (failures >= failure_limit)
            {
              printf ("warning: bailing out after %d failures\n", failures);
              return;
            }
        }
    }
  printf ("info: key generation finished with %d failures\n", failures);

  memset (&byte_counts, 0, sizeof (byte_counts));
  for (int key = 0; key < key_count; ++key)
    for (int pos = 0; pos < arc4random_key_size; ++pos)
      ++byte_counts[pos][keys[key].data[pos]];

  for (int pos = 0; pos < arc4random_key_size; ++pos)
    for (int byte = 0; byte < 256; ++byte)
      if (byte_counts[pos][byte] == 0)
        {
          support_record_failure ();
          printf ("error: byte %d never appeared at position %d\n", byte, pos);
        }
}

/* Test adapter for arc4random.  */
static bool
generate_arc4random (unsigned char *key)
{
  uint32_t words[arc4random_key_size / 4];
  _Static_assert (sizeof (words) == arc4random_key_size, "sizeof (words)");

  for (int i = 0; i < array_length (words); ++i)
    words[i] = arc4random ();
  memcpy (key, &words, arc4random_key_size);
  return true;
}

/* Test adapter for arc4random_buf.  */
static bool
generate_arc4random_buf (unsigned char *key)
{
  arc4random_buf (key, arc4random_key_size);
  return true;
}

/* Test adapter for arc4random_uniform.  */
static bool
generate_arc4random_uniform (unsigned char *key)
{
  for (int i = 0; i < arc4random_key_size; ++i)
    key[i] = arc4random_uniform (256);
  return true;
}

/* Test adapter for arc4random_uniform with argument 257.  This means
   that byte 0 happens more often, but we do not perform such a
   statistical check, so the test will still pass */
static bool
generate_arc4random_uniform_257 (unsigned char *key)
{
  for (int i = 0; i < arc4random_key_size; ++i)
    key[i] = arc4random_uniform (257);
  return true;
}

static int
do_test (void)
{
  puts ("info: arc4random implementation test");
  find_stuck_bytes (generate_arc4random);

  puts ("info: arc4random_buf implementation test");
  find_stuck_bytes (generate_arc4random_buf);

  puts ("info: arc4random_uniform implementation test");
  find_stuck_bytes (generate_arc4random_uniform);

  puts ("info: arc4random_uniform implementation test (257 variant)");
  find_stuck_bytes (generate_arc4random_uniform_257);

  return 0;
}

#include <support/test-driver.c>
