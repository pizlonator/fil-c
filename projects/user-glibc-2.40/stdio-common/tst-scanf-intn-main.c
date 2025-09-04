/* Test scanf formats for intN_t, int_leastN_t and int_fastN_t types.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <libc-diag.h>
#include <support/check.h>

/* GCC does not know the %wN or %wfN length modifiers before GCC 13.  */
DIAG_PUSH_NEEDS_COMMENT;
#if !__GNUC_PREREQ (13, 0)
DIAG_IGNORE_NEEDS_COMMENT (12, "-Wformat");
DIAG_IGNORE_NEEDS_COMMENT (12, "-Wformat-extra-args");
#endif

#define CHECK_SCANF1(EXPECTED, STR, FMT)				\
  do									\
    {									\
      var = ((typeof (var)) 0xabababab);				\
      int ret = SSCANF (L_(STR), L_(FMT), &var);			\
      TEST_COMPARE (var, (EXPECTED));					\
      TEST_COMPARE (ret, 1);						\
    }									\
  while (0)

#define CHECK_SCANF1N(EXPECTED, STR, FMT)				\
  do									\
    {									\
      var = ((typeof (var)) 0xabababab);				\
      n = 123;								\
      int ret = SSCANF (L_(STR), L_(FMT), &var, &n);			\
      TEST_COMPARE (var, (EXPECTED));					\
      TEST_COMPARE (n, STRLEN (L_(STR)));				\
      TEST_COMPARE (ret, 1);						\
    }									\
  while (0)

#define CHECK_SCANF_ERR(OK, STR, FMT, ...)				\
  do									\
    {									\
      int ret = SSCANF (L_(STR), L_(FMT), __VA_ARGS__);			\
      TEST_VERIFY (ret == (OK));					\
      TEST_COMPARE (errno, EINVAL);					\
    }									\
  while (0)

static void
test_w8 (void)
{
  {
    int8_t var, n;
    CHECK_SCANF1 (42, "42", "%w8d");
    CHECK_SCANF1N (42, "42", "%w8d%w8n");
    CHECK_SCANF1 (-43, "-43", "%w8d");
    CHECK_SCANF1 (42, "42", "%w8i");
    CHECK_SCANF1 (-43, "-43", "%w8i");
    CHECK_SCANF1 (123, "0b1111011", "%w8i");
    CHECK_SCANF1 (127, "0x7f", "%w8i");
    CHECK_SCANF1 (-19, "-023", "%w8i");
  }
  {
    uint8_t var;
    int8_t n;
    CHECK_SCANF1 (123, "1111011", "%w8b");
    CHECK_SCANF1 (19, "023", "%w8o");
    CHECK_SCANF1 (50, "50", "%w8u");
    CHECK_SCANF1 (65, "41", "%w8x");
    CHECK_SCANF1N (65, "41", "%w8x%w8n");
    CHECK_SCANF1 (66, "42", "%w8X");
  }
  {
    int_least8_t var, n;
    CHECK_SCANF1 (42, "42", "%w8d");
    CHECK_SCANF1N (42, "42", "%w8d%w8n");
    CHECK_SCANF1 (-43, "-43", "%w8d");
    CHECK_SCANF1 (42, "42", "%w8i");
    CHECK_SCANF1 (-43, "-43", "%w8i");
    CHECK_SCANF1 (123, "0b1111011", "%w8i");
    CHECK_SCANF1 (127, "0x7f", "%w8i");
    CHECK_SCANF1 (-19, "-023", "%w8i");
  }
  {
    uint_least8_t var;
    int_least8_t n;
    CHECK_SCANF1 (123, "1111011", "%w8b");
    CHECK_SCANF1 (19, "023", "%w8o");
    CHECK_SCANF1 (50, "50", "%w8u");
    CHECK_SCANF1 (65, "41", "%w8x");
    CHECK_SCANF1N (65, "41", "%w8x%w8n");
    CHECK_SCANF1 (66, "42", "%w8X");
  }
}

static void
test_wf8 (void)
{
  {
    int_fast8_t var, n;
    CHECK_SCANF1 (42, "42", "%wf8d");
    CHECK_SCANF1N (42, "42", "%wf8d%wf8n");
    CHECK_SCANF1 (-43, "-43", "%wf8d");
    CHECK_SCANF1 (42, "42", "%wf8i");
    CHECK_SCANF1 (-43, "-43", "%wf8i");
    CHECK_SCANF1 (123, "0b1111011", "%wf8i");
    CHECK_SCANF1 (127, "0x7f", "%wf8i");
    CHECK_SCANF1 (-19, "-023", "%wf8i");
  }
  {
    uint_fast8_t var;
    int_fast8_t n;
    CHECK_SCANF1 (123, "1111011", "%wf8b");
    CHECK_SCANF1 (19, "023", "%wf8o");
    CHECK_SCANF1 (50, "50", "%wf8u");
    CHECK_SCANF1 (65, "41", "%wf8x");
    CHECK_SCANF1N (65, "41", "%wf8x%wf8n");
    CHECK_SCANF1 (66, "42", "%wf8X");
  }
}

static void
test_w16 (void)
{
  {
    int16_t var, n;
    CHECK_SCANF1 (12345, "12345", "%w16d");
    CHECK_SCANF1N (23456, "23456", "%w16d%w16n");
    CHECK_SCANF1 (-10101, "-10101", "%w16d");
    CHECK_SCANF1 (30000, "30000", "%w16i");
    CHECK_SCANF1 (-19876, "-19876", "%w16i");
    CHECK_SCANF1 (16384, "0b100000000000000", "%w16i");
    CHECK_SCANF1 (32767, "0x7fff", "%w16i");
    CHECK_SCANF1 (-16383, "-037777", "%w16i");
  }
  {
    uint16_t var;
    int16_t n;
    CHECK_SCANF1 (32767, "111111111111111", "%w16b");
    CHECK_SCANF1 (4095, "07777", "%w16o");
    CHECK_SCANF1 (9999, "9999", "%w16u");
    CHECK_SCANF1 (23456, "5ba0", "%w16x");
    CHECK_SCANF1N (23456, "5ba0", "%w16x%w16n");
    CHECK_SCANF1 (23457, "5ba1", "%w16X");
  }
  {
    int_least16_t var, n;
    CHECK_SCANF1 (12345, "12345", "%w16d");
    CHECK_SCANF1N (23456, "23456", "%w16d%w16n");
    CHECK_SCANF1 (-10101, "-10101", "%w16d");
    CHECK_SCANF1 (30000, "30000", "%w16i");
    CHECK_SCANF1 (-19876, "-19876", "%w16i");
    CHECK_SCANF1 (16384, "0b100000000000000", "%w16i");
    CHECK_SCANF1 (32767, "0x7fff", "%w16i");
    CHECK_SCANF1 (-16383, "-037777", "%w16i");
  }
  {
    uint_least16_t var;
    int_least16_t n;
    CHECK_SCANF1 (32767, "111111111111111", "%w16b");
    CHECK_SCANF1 (4095, "07777", "%w16o");
    CHECK_SCANF1 (9999, "9999", "%w16u");
    CHECK_SCANF1 (23456, "5ba0", "%w16x");
    CHECK_SCANF1N (23456, "5ba0", "%w16x%w16n");
    CHECK_SCANF1 (23457, "5ba1", "%w16X");
  }
}

static void
test_wf16 (void)
{
  {
    int_fast16_t var, n;
    CHECK_SCANF1 (12345, "12345", "%wf16d");
    CHECK_SCANF1N (23456, "23456", "%wf16d%wf16n");
    CHECK_SCANF1 (-10101, "-10101", "%wf16d");
    CHECK_SCANF1 (30000, "30000", "%wf16i");
    CHECK_SCANF1 (-19876, "-19876", "%wf16i");
    CHECK_SCANF1 (16384, "0b100000000000000", "%wf16i");
    CHECK_SCANF1 (32767, "0x7fff", "%wf16i");
    CHECK_SCANF1 (-16383, "-037777", "%wf16i");
  }
  {
    uint_fast16_t var;
    int_fast16_t n;
    CHECK_SCANF1 (32767, "111111111111111", "%wf16b");
    CHECK_SCANF1 (4095, "07777", "%wf16o");
    CHECK_SCANF1 (9999, "9999", "%wf16u");
    CHECK_SCANF1 (23456, "5ba0", "%wf16x");
    CHECK_SCANF1N (23456, "5ba0", "%wf16x%wf16n");
    CHECK_SCANF1 (23457, "5ba1", "%wf16X");
  }
#if INT_FAST16_MAX >= INT32_MAX
  {
    int_fast16_t var, n;
    CHECK_SCANF1 (1234567, "1234567", "%wf16d");
    CHECK_SCANF1N (2345678, "2345678", "%wf16d%wf16n");
    CHECK_SCANF1 (-1010101, "-1010101", "%wf16d");
    CHECK_SCANF1 (3000000, "3000000", "%wf16i");
    CHECK_SCANF1 (-98765432, "-98765432", "%wf16i");
    CHECK_SCANF1 (1048576, "0b100000000000000000000", "%wf16i");
    CHECK_SCANF1 (1048575, "0xfffff", "%wf16i");
    CHECK_SCANF1 (-1048575, "-03777777", "%wf16i");
  }
  {
    uint_fast16_t var;
    int_fast16_t n;
    CHECK_SCANF1 (1234567, "100101101011010000111", "%wf16b");
    CHECK_SCANF1 (1048575, "03777777", "%wf16o");
    CHECK_SCANF1 (999999, "999999", "%wf16u");
    CHECK_SCANF1 (987654, "f1206", "%wf16x");
    CHECK_SCANF1N (987654, "f1206", "%wf16x%wf16n");
    CHECK_SCANF1 (987655, "f1207", "%wf16X");
  }
#endif
#if INT_FAST16_MAX >= INT64_MAX
  {
    int_fast16_t var, n;
    CHECK_SCANF1 (123456789012LL, "123456789012", "%wf16d");
    CHECK_SCANF1N (234567890123LL, "234567890123", "%wf16d%wf16n");
    CHECK_SCANF1 (-10101010101LL, "-10101010101", "%wf16d");
    CHECK_SCANF1 (3000000000000000LL, "3000000000000000", "%wf16i");
    CHECK_SCANF1 (-9876543210LL, "-9876543210", "%wf16i");
    CHECK_SCANF1 (1LL << 40, "0b10000000000000000000000000000000000000000",
		  "%wf16i");
    CHECK_SCANF1 (1LL << 41, "0x20000000000", "%wf16i");
    CHECK_SCANF1 (-(1LL << 42), "-0100000000000000", "%wf16i");
  }
  {
    uint_fast16_t var;
    int_fast16_t n;
    CHECK_SCANF1 (123456789012ULL, "1110010111110100110010001101000010100",
		  "%wf16b");
    CHECK_SCANF1 (1ULL << 40, "20000000000000", "%wf16o");
    CHECK_SCANF1 (999999999999ULL, "999999999999", "%wf16u");
    CHECK_SCANF1 (9876543210ULL, "24cb016ea", "%wf16x");
    CHECK_SCANF1N (9876543210ULL, "24cb016ea", "%wf16x%wf16n");
    CHECK_SCANF1 (9876543211ULL, "24cb016eb", "%wf16X");
  }
#endif
}

static void
test_w32 (void)
{
  {
    int32_t var, n;
    CHECK_SCANF1 (1234567, "1234567", "%w32d");
    CHECK_SCANF1N (2345678, "2345678", "%w32d%w32n");
    CHECK_SCANF1 (-1010101, "-1010101", "%w32d");
    CHECK_SCANF1 (3000000, "3000000", "%w32i");
    CHECK_SCANF1 (-98765432, "-98765432", "%w32i");
    CHECK_SCANF1 (1048576, "0b100000000000000000000", "%w32i");
    CHECK_SCANF1 (1048575, "0xfffff", "%w32i");
    CHECK_SCANF1 (-1048575, "-03777777", "%w32i");
  }
  {
    uint32_t var;
    int32_t n;
    CHECK_SCANF1 (1234567, "100101101011010000111", "%w32b");
    CHECK_SCANF1 (1048575, "03777777", "%w32o");
    CHECK_SCANF1 (999999, "999999", "%w32u");
    CHECK_SCANF1 (987654, "f1206", "%w32x");
    CHECK_SCANF1N (987654, "f1206", "%w32x%w32n");
    CHECK_SCANF1 (987655, "f1207", "%w32X");
  }
  {
    int_least32_t var, n;
    CHECK_SCANF1 (1234567, "1234567", "%w32d");
    CHECK_SCANF1N (2345678, "2345678", "%w32d%w32n");
    CHECK_SCANF1 (-1010101, "-1010101", "%w32d");
    CHECK_SCANF1 (3000000, "3000000", "%w32i");
    CHECK_SCANF1 (-98765432, "-98765432", "%w32i");
    CHECK_SCANF1 (1048576, "0b100000000000000000000", "%w32i");
    CHECK_SCANF1 (1048575, "0xfffff", "%w32i");
    CHECK_SCANF1 (-1048575, "-03777777", "%w32i");
  }
  {
    uint_least32_t var;
    int_least32_t n;
    CHECK_SCANF1 (1234567, "100101101011010000111", "%w32b");
    CHECK_SCANF1 (1048575, "03777777", "%w32o");
    CHECK_SCANF1 (999999, "999999", "%w32u");
    CHECK_SCANF1 (987654, "f1206", "%w32x");
    CHECK_SCANF1N (987654, "f1206", "%w32x%w32n");
    CHECK_SCANF1 (987655, "f1207", "%w32X");
  }
}

static void
test_wf32 (void)
{
  {
    int_fast32_t var, n;
    CHECK_SCANF1 (1234567, "1234567", "%wf32d");
    CHECK_SCANF1N (2345678, "2345678", "%wf32d%wf32n");
    CHECK_SCANF1 (-1010101, "-1010101", "%wf32d");
    CHECK_SCANF1 (3000000, "3000000", "%wf32i");
    CHECK_SCANF1 (-98765432, "-98765432", "%wf32i");
    CHECK_SCANF1 (1048576, "0b100000000000000000000", "%wf32i");
    CHECK_SCANF1 (1048575, "0xfffff", "%wf32i");
    CHECK_SCANF1 (-1048575, "-03777777", "%wf32i");
  }
  {
    uint_fast32_t var;
    int_fast32_t n;
    CHECK_SCANF1 (1234567, "100101101011010000111", "%wf32b");
    CHECK_SCANF1 (1048575, "03777777", "%wf32o");
    CHECK_SCANF1 (999999, "999999", "%wf32u");
    CHECK_SCANF1 (987654, "f1206", "%wf32x");
    CHECK_SCANF1N (987654, "f1206", "%wf32x%wf32n");
    CHECK_SCANF1 (987655, "f1207", "%wf32X");
  }
#if INT_FAST32_MAX >= INT64_MAX
  {
    int_fast32_t var, n;
    CHECK_SCANF1 (123456789012LL, "123456789012", "%wf32d");
    CHECK_SCANF1N (234567890123LL, "234567890123", "%wf32d%wf32n");
    CHECK_SCANF1 (-10101010101LL, "-10101010101", "%wf32d");
    CHECK_SCANF1 (3000000000000000LL, "3000000000000000", "%wf32i");
    CHECK_SCANF1 (-9876543210LL, "-9876543210", "%wf32i");
    CHECK_SCANF1 (1LL << 40, "0b10000000000000000000000000000000000000000",
		  "%wf32i");
    CHECK_SCANF1 (1LL << 41, "0x20000000000", "%wf32i");
    CHECK_SCANF1 (-(1LL << 42), "-0100000000000000", "%wf32i");
  }
  {
    uint_fast32_t var;
    int_fast32_t n;
    CHECK_SCANF1 (123456789012ULL, "1110010111110100110010001101000010100",
		  "%wf32b");
    CHECK_SCANF1 (1ULL << 40, "20000000000000", "%wf32o");
    CHECK_SCANF1 (999999999999ULL, "999999999999", "%wf32u");
    CHECK_SCANF1 (9876543210ULL, "24cb016ea", "%wf32x");
    CHECK_SCANF1N (9876543210ULL, "24cb016ea", "%wf32x%wf32n");
    CHECK_SCANF1 (9876543211ULL, "24cb016eb", "%wf32X");
  }
#endif
}

static void
test_w64 (void)
{
  {
    int64_t var, n;
    CHECK_SCANF1 (123456789012LL, "123456789012", "%w64d");
    CHECK_SCANF1N (234567890123LL, "234567890123", "%w64d%w64n");
    CHECK_SCANF1 (-10101010101LL, "-10101010101", "%w64d");
    CHECK_SCANF1 (3000000000000000LL, "3000000000000000", "%w64i");
    CHECK_SCANF1 (-9876543210LL, "-9876543210", "%w64i");
    CHECK_SCANF1 (1LL << 40, "0b10000000000000000000000000000000000000000",
		  "%w64i");
    CHECK_SCANF1 (1LL << 41, "0x20000000000", "%w64i");
    CHECK_SCANF1 (-(1LL << 42), "-0100000000000000", "%w64i");
  }
  {
    uint64_t var;
    int64_t n;
    CHECK_SCANF1 (123456789012ULL, "1110010111110100110010001101000010100",
		  "%w64b");
    CHECK_SCANF1 (1ULL << 40, "20000000000000", "%w64o");
    CHECK_SCANF1 (999999999999ULL, "999999999999", "%w64u");
    CHECK_SCANF1 (9876543210ULL, "24cb016ea", "%w64x");
    CHECK_SCANF1N (9876543210ULL, "24cb016ea", "%w64x%w64n");
    CHECK_SCANF1 (9876543211ULL, "24cb016eb", "%w64X");
  }
  {
    int_least64_t var, n;
    CHECK_SCANF1 (123456789012LL, "123456789012", "%w64d");
    CHECK_SCANF1N (234567890123LL, "234567890123", "%w64d%w64n");
    CHECK_SCANF1 (-10101010101LL, "-10101010101", "%w64d");
    CHECK_SCANF1 (3000000000000000LL, "3000000000000000", "%w64i");
    CHECK_SCANF1 (-9876543210LL, "-9876543210", "%w64i");
    CHECK_SCANF1 (1LL << 40, "0b10000000000000000000000000000000000000000",
		  "%w64i");
    CHECK_SCANF1 (1LL << 41, "0x20000000000", "%w64i");
    CHECK_SCANF1 (-(1LL << 42), "-0100000000000000", "%w64i");
  }
  {
    uint_least64_t var;
    int_least64_t n;
    CHECK_SCANF1 (123456789012ULL, "1110010111110100110010001101000010100",
		  "%w64b");
    CHECK_SCANF1 (1ULL << 40, "20000000000000", "%w64o");
    CHECK_SCANF1 (999999999999ULL, "999999999999", "%w64u");
    CHECK_SCANF1 (9876543210ULL, "24cb016ea", "%w64x");
    CHECK_SCANF1N (9876543210ULL, "24cb016ea", "%w64x%w64n");
    CHECK_SCANF1 (9876543211ULL, "24cb016eb", "%w64X");
  }
}

static void
test_wf64 (void)
{
  {
    int_fast64_t var, n;
    CHECK_SCANF1 (123456789012LL, "123456789012", "%wf64d");
    CHECK_SCANF1N (234567890123LL, "234567890123", "%wf64d%wf64n");
    CHECK_SCANF1 (-10101010101LL, "-10101010101", "%wf64d");
    CHECK_SCANF1 (3000000000000000LL, "3000000000000000", "%wf64i");
    CHECK_SCANF1 (-9876543210LL, "-9876543210", "%wf64i");
    CHECK_SCANF1 (1LL << 40, "0b10000000000000000000000000000000000000000",
		  "%wf64i");
    CHECK_SCANF1 (1LL << 41, "0x20000000000", "%wf64i");
    CHECK_SCANF1 (-(1LL << 42), "-0100000000000000", "%wf64i");
  }
  {
    uint_fast64_t var;
    int_fast64_t n;
    CHECK_SCANF1 (123456789012ULL, "1110010111110100110010001101000010100",
		  "%wf64b");
    CHECK_SCANF1 (1ULL << 40, "20000000000000", "%wf64o");
    CHECK_SCANF1 (999999999999ULL, "999999999999", "%wf64u");
    CHECK_SCANF1 (9876543210ULL, "24cb016ea", "%wf64x");
    CHECK_SCANF1N (9876543210ULL, "24cb016ea", "%wf64x%wf64n");
    CHECK_SCANF1 (9876543211ULL, "24cb016eb", "%wf64X");
  }
}

static int
do_test (void)
{
  int a, b;
  test_w8 ();
  test_wf8 ();
  test_w16 ();
  test_wf16 ();
  test_w32 ();
  test_wf32 ();
  test_w64 ();
  test_wf64 ();
  /* Bad N in %wN and %wfN are required to produce an error return (of
     the number of input items assigned) from scanf functions (and can
     also be seen to be invalid at compile time).  */
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (13, "-Wformat");
  DIAG_IGNORE_NEEDS_COMMENT (13, "-Wformat-extra-args");
  CHECK_SCANF_ERR (0, "1", "%w1d", &a);
  CHECK_SCANF_ERR (0, "1", "%w123d", &a);
  CHECK_SCANF_ERR (0, "1", "%w99999999999999999999d", &a);
  CHECK_SCANF_ERR (0, "1", "%wf1d", &a);
  CHECK_SCANF_ERR (0, "1", "%wf123d", &a);
  CHECK_SCANF_ERR (0, "1", "%wf99999999999999999999d", &a);
  CHECK_SCANF_ERR (1, "1 1", "%d %w1d", &a, &b);
  CHECK_SCANF_ERR (1, "1 1", "%d %w123d", &a, &b);
  CHECK_SCANF_ERR (1, "1 1", "%d %w99999999999999999999d", &a, &b);
  CHECK_SCANF_ERR (1, "1 1", "%d %wf1d", &a, &b);
  CHECK_SCANF_ERR (1, "1 1", "%d %wf123d", &a, &b);
  CHECK_SCANF_ERR (1, "1 1", "%d %wf99999999999999999999d", &a, &b);
  CHECK_SCANF_ERR (0, "1", "%1$w1d", &a);
  CHECK_SCANF_ERR (0, "1", "%1$w123d", &a);
  CHECK_SCANF_ERR (0, "1", "%1$w99999999999999999999d", &a);
  CHECK_SCANF_ERR (0, "1", "%1$wf1d", &a);
  CHECK_SCANF_ERR (0, "1", "%1$wf123d", &a);
  CHECK_SCANF_ERR (0, "1", "%1$wf99999999999999999999d", &a);
  DIAG_POP_NEEDS_COMMENT;
  return 0;
}

DIAG_POP_NEEDS_COMMENT;

#include <support/test-driver.c>
