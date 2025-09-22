/* Test program for leb128
   Copyright (C) 2020 Tom Tromey
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <libdw.h>
#include "../libdw/libdwP.h"
#include "../libdw/memory-access.h"

#define OK 0
#define FAIL 1

static const unsigned char v0[] = { 0x0 };
static const unsigned char v1[] = { 0x1 };
static const unsigned char v23[] = { 23 };
static const unsigned char vm_1[] = { 0x7f };
static const unsigned char vm_2[] = { 0x7e };
static const unsigned char s127[] = { 0xff, 0x00 };
static const unsigned char v128[] = { 0x80, 0x01 };
static const unsigned char v129[] = { 0x81, 0x01 };
static const unsigned char vm_127[] = { 0x81, 0x7f };
static const unsigned char vm_128[] = { 0x80, 0x7f };
static const unsigned char vm_129[] = { 0xff, 0x7e };
static const unsigned char vhuge[] =
  {
    0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x0
  };
static const unsigned char most_positive[] =
  {
    0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3f
  };
static const unsigned char most_negative[] =
  {
    0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x40
  };
static const unsigned char minus_one[] =
  {
    0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x7f
  };
static const unsigned char int64_max_m1[] =
  {
    0xfe, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00
  };
static const unsigned char int64_min_p1[] =
  {
    0x81, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x7f
  };

static int
test_one_sleb (const unsigned char *data, size_t len, int64_t expect)
{
  int64_t value;
  const unsigned char *p;

  p = data;
  get_sleb128 (value, p, p + len);
  if (value != expect || p != data + len)
    return FAIL;

  p = data;
  get_sleb128_unchecked (value, p);
  if (value != expect || p != data + len)
    return FAIL;

  return OK;
}

static int
test_sleb (void)
{
#define TEST(ARRAY, V)				      \
  if (test_one_sleb (ARRAY, sizeof (ARRAY), V) != OK) \
    return FAIL;

  TEST (v0, 0);
  TEST (v1, 1);
  TEST (v23, 23);
  TEST (vm_1, -1);
  TEST (vm_2, -2);
  TEST (s127, 127);
  TEST (v128, 128);
  TEST (v129, 129);
  TEST (vm_127, -127);
  TEST (vm_128, -128);
  TEST (vm_129, -129);
  TEST (vhuge, 9223372036854775807ll);
  TEST (most_positive, 4611686018427387903ll);
  TEST (most_negative, -4611686018427387904ll);
  TEST (minus_one, -1);
  TEST (int64_max_m1, INT64_MAX - 1);
  TEST (int64_min_p1, INT64_MIN + 1);

#undef TEST

  return OK;
}

static int
test_sleb_safety (void)
{
  const int64_t expected_error = INT64_MAX;
  int64_t value;
  const unsigned char *test = NULL;
  get_sleb128 (value, test, test);
  if (value != expected_error)
    return FAIL;

  return OK;
}

static int
test_one_uleb (const unsigned char *data, size_t len, uint64_t expect)
{
  uint64_t value;
  const unsigned char *p;

  p = data;
  get_uleb128 (value, p, p + len);
  if (value != expect || p != data + len)
    return FAIL;

  p = data;
  get_uleb128_unchecked (value, p);
  if (value != expect || p != data + len)
    return FAIL;

  return OK;
}

static int
test_uleb (void)
{
#define TEST(ARRAY, V)				      \
  if (test_one_uleb (ARRAY, sizeof (ARRAY), V) != OK) \
    return FAIL;

  TEST (v0, 0);
  TEST (v1, 1);
  TEST (v23, 23);
  TEST (vm_1, 127);
  TEST (vm_2, 126);
  TEST (s127, 127);
  TEST (v128, 128);
  TEST (v129, 129);
  TEST (vm_127, 16257);
  TEST (vm_128, 16256);
  TEST (vm_129, 16255);
  TEST (vhuge, 9223372036854775807ull);
  TEST (most_positive, 4611686018427387903ull);
  TEST (most_negative, 4611686018427387904ull);
  TEST (minus_one, 9223372036854775807ull);
  TEST (int64_max_m1, INT64_MAX - 1);
  TEST (int64_min_p1, INT64_MIN + 1);

#undef TEST

  return OK;
}

static int
test_uleb_safety (void)
{
  const uint64_t expected_error = UINT64_MAX;
  uint64_t value;
  const unsigned char *test = NULL;
  get_uleb128 (value, test, test);
  if (value != expected_error)
    return FAIL;

  return OK;
}

int
main (void)
{
  return test_sleb () || test_sleb_safety () || test_uleb ()
	 || test_uleb_safety ();
}
