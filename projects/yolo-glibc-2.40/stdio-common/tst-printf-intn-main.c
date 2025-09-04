/* Test printf formats for intN_t, int_leastN_t and int_fastN_t types.
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
#include <inttypes.h>
#include <limits.h>
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

#define CHECK_PRINTF(EXPECTED, FMT, ...)				\
  do									\
    {									\
      int ret = SNPRINTF (buf, sizeof buf / sizeof buf[0], L_(FMT),	\
			  __VA_ARGS__);					\
      TEST_COMPARE_STRING_MACRO (buf, L_(EXPECTED));			\
      TEST_COMPARE (ret, STRLEN (L_(EXPECTED)));			\
    }									\
  while (0)

#define CHECK_PRINTF_ERR(FMT, ...)					\
  do									\
    {									\
      int ret = SNPRINTF (buf, sizeof buf / sizeof buf[0], L_(FMT),	\
			  __VA_ARGS__);					\
      TEST_VERIFY (ret < 0);						\
      TEST_COMPARE (errno, EINVAL);					\
    }									\
  while (0)

static void
test_w8 (void)
{
  CHAR buf[1024];
  CHECK_PRINTF ("123", "%w8d", (int8_t) 123);
  CHECK_PRINTF ("-123", "%w8d", (int8_t) -123);
  CHECK_PRINTF ("123", "%w8i", (int8_t) 123);
  CHECK_PRINTF ("-123", "%w8i", (int8_t) -123);
  CHECK_PRINTF ("1111011", "%w8b", (uint8_t) 123);
  CHECK_PRINTF ("1111011", "%w8B", (uint8_t) 123);
  CHECK_PRINTF ("173", "%w8o", (uint8_t) 123);
  CHECK_PRINTF ("123", "%w8u", (uint8_t) 123);
  CHECK_PRINTF ("7b", "%w8x", (uint8_t) 123);
  CHECK_PRINTF ("7B", "%w8X", (uint8_t) 123);
  CHECK_PRINTF ("  123", "%5w8d", (int8_t) 123);
  CHECK_PRINTF ("  123", "%*w8d", 5, (int8_t) 123);
  CHECK_PRINTF ("0x7b", "%#w8x", (uint8_t) 123);
  CHECK_PRINTF ("00123", "%.5w8d", (int8_t) 123);
  CHECK_PRINTF ("00123", "%.*w8d", 5, (int8_t) 123);
  CHECK_PRINTF ("   00123", "%8.5w8d", (int8_t) 123);
  CHECK_PRINTF ("   00123", "%*.5w8d", 8, (int8_t) 123);
  CHECK_PRINTF ("   00123", "%8.*w8d", 5, (int8_t) 123);
  CHECK_PRINTF ("   00123", "%*.*w8d", 8, 5, (int8_t) 123);
  CHECK_PRINTF ("00123   ", "%-8.5w8d", (int8_t) 123);
  CHECK_PRINTF ("00123   ", "%-*.5w8d", 8, (int8_t) 123);
  CHECK_PRINTF ("00123   ", "%-8.*w8d", 5, (int8_t) 123);
  CHECK_PRINTF ("00123   ", "%-*.*w8d", 8, 5, (int8_t) 123);
  {
    int8_t n = -1;
    CHECK_PRINTF ("12345", "%d%w8n", 12345, &n);
    TEST_COMPARE (n, 5);
  }
  CHECK_PRINTF ("123", "%w8d", (int_least8_t) 123);
  CHECK_PRINTF ("-123", "%w8d", (int_least8_t) -123);
  CHECK_PRINTF ("123", "%w8i", (int_least8_t) 123);
  CHECK_PRINTF ("-123", "%w8i", (int_least8_t) -123);
  CHECK_PRINTF ("1111011", "%w8b", (uint_least8_t) 123);
  CHECK_PRINTF ("1111011", "%w8B", (uint_least8_t) 123);
  CHECK_PRINTF ("173", "%w8o", (uint_least8_t) 123);
  CHECK_PRINTF ("123", "%w8u", (uint_least8_t) 123);
  CHECK_PRINTF ("7b", "%w8x", (uint_least8_t) 123);
  CHECK_PRINTF ("7B", "%w8X", (uint_least8_t) 123);
  CHECK_PRINTF ("  123", "%5w8d", (int_least8_t) 123);
  CHECK_PRINTF ("  123", "%*w8d", 5, (int_least8_t) 123);
  CHECK_PRINTF ("0x7b", "%#w8x", (uint_least8_t) 123);
  CHECK_PRINTF ("00123", "%.5w8d", (int_least8_t) 123);
  CHECK_PRINTF ("00123", "%.*w8d", 5, (int_least8_t) 123);
  CHECK_PRINTF ("   00123", "%8.5w8d", (int_least8_t) 123);
  CHECK_PRINTF ("   00123", "%*.5w8d", 8, (int_least8_t) 123);
  CHECK_PRINTF ("   00123", "%8.*w8d", 5, (int_least8_t) 123);
  CHECK_PRINTF ("   00123", "%*.*w8d", 8, 5, (int_least8_t) 123);
  CHECK_PRINTF ("00123   ", "%-8.5w8d", (int_least8_t) 123);
  CHECK_PRINTF ("00123   ", "%-*.5w8d", 8, (int_least8_t) 123);
  CHECK_PRINTF ("00123   ", "%-8.*w8d", 5, (int_least8_t) 123);
  CHECK_PRINTF ("00123   ", "%-*.*w8d", 8, 5, (int_least8_t) 123);
  {
    int_least8_t ln = -1;
    CHECK_PRINTF ("12345", "%d%w8n", 12345, &ln);
    TEST_COMPARE (ln, 5);
  }
  /* Test truncation of value in promoted type not representable in
     narrower type.  */
  CHECK_PRINTF ("57", "%w8d", 12345);
  CHECK_PRINTF ("-57", "%w8d", -12345);
  CHECK_PRINTF ("-121", "%w8d", 1234567);
  CHECK_PRINTF ("121", "%w8d", -1234567);
  CHECK_PRINTF ("135", "%w8u", 1234567);
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 10 test2 20", "%4$s %3$w8d %2$s %1$w8d",
		276, "test2", 266, "test");
}

static void
test_wf8 (void)
{
  CHAR buf[1024];
  _Static_assert (sizeof (int_fast8_t) == sizeof (char),
		  "test assumes size of int_fast8_t");
  CHECK_PRINTF ("123", "%wf8d", (int_fast8_t) 123);
  CHECK_PRINTF ("-123", "%wf8d", (int_fast8_t) -123);
  CHECK_PRINTF ("123", "%wf8i", (int_fast8_t) 123);
  CHECK_PRINTF ("-123", "%wf8i", (int_fast8_t) -123);
  CHECK_PRINTF ("1111011", "%wf8b", (uint_fast8_t) 123);
  CHECK_PRINTF ("1111011", "%wf8B", (uint_fast8_t) 123);
  CHECK_PRINTF ("173", "%wf8o", (uint_fast8_t) 123);
  CHECK_PRINTF ("123", "%wf8u", (uint_fast8_t) 123);
  CHECK_PRINTF ("7b", "%wf8x", (uint_fast8_t) 123);
  CHECK_PRINTF ("7B", "%wf8X", (uint_fast8_t) 123);
  CHECK_PRINTF ("  123", "%5w8d", (int_fast8_t) 123);
  CHECK_PRINTF ("  123", "%*w8d", 5, (int_fast8_t) 123);
  CHECK_PRINTF ("0x7b", "%#w8x", (uint_fast8_t) 123);
  CHECK_PRINTF ("00123", "%.5w8d", (int_fast8_t) 123);
  CHECK_PRINTF ("00123", "%.*w8d", 5, (int_fast8_t) 123);
  CHECK_PRINTF ("   00123", "%8.5w8d", (int_fast8_t) 123);
  CHECK_PRINTF ("   00123", "%*.5w8d", 8, (int_fast8_t) 123);
  CHECK_PRINTF ("   00123", "%8.*w8d", 5, (int_fast8_t) 123);
  CHECK_PRINTF ("   00123", "%*.*w8d", 8, 5, (int_fast8_t) 123);
  CHECK_PRINTF ("00123   ", "%-8.5w8d", (int_fast8_t) 123);
  CHECK_PRINTF ("00123   ", "%-*.5w8d", 8, (int_fast8_t) 123);
  CHECK_PRINTF ("00123   ", "%-8.*w8d", 5, (int_fast8_t) 123);
  CHECK_PRINTF ("00123   ", "%-*.*w8d", 8, 5, (int_fast8_t) 123);
  {
    int_fast8_t n = -1;
    CHECK_PRINTF ("12345", "%d%wf8n", 12345, &n);
    TEST_COMPARE (n, 5);
  }
  /* Test truncation of value in promoted type not representable in
     narrower type.  */
  CHECK_PRINTF ("57", "%wf8d", 12345);
  CHECK_PRINTF ("-57", "%wf8d", -12345);
  CHECK_PRINTF ("-121", "%wf8d", 1234567);
  CHECK_PRINTF ("121", "%wf8d", -1234567);
  CHECK_PRINTF ("135", "%wf8u", 1234567);
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 10 test2 20", "%4$s %3$wf8d %2$s %1$wf8d",
		276, "test2", 266, "test");
}

static void
test_w16 (void)
{
  CHAR buf[1024];
  CHECK_PRINTF ("12345", "%w16d", (int16_t) 12345);
  CHECK_PRINTF ("-12345", "%w16d", (int16_t) -12345);
  CHECK_PRINTF ("12345", "%w16i", (int16_t) 12345);
  CHECK_PRINTF ("-12345", "%w16i", (int16_t) -12345);
  CHECK_PRINTF ("11000000111001", "%w16b", (uint16_t) 12345);
  CHECK_PRINTF ("11000000111001", "%w16B", (uint16_t) 12345);
  CHECK_PRINTF ("30071", "%w16o", (uint16_t) 12345);
  CHECK_PRINTF ("12345", "%w16u", (uint16_t) 12345);
  CHECK_PRINTF ("303a", "%w16x", (uint16_t) 12346);
  CHECK_PRINTF ("303A", "%w16X", (uint16_t) 12346);
  CHECK_PRINTF ("  12345", "%7w16d", (int16_t) 12345);
  CHECK_PRINTF ("  12345", "%*w16d", 7, (int16_t) 12345);
  CHECK_PRINTF ("0x3039", "%#w16x", (uint16_t) 12345);
  CHECK_PRINTF ("0012345", "%.7w16d", (int16_t) 12345);
  CHECK_PRINTF ("0012345", "%.*w16d", 7, (int16_t) 12345);
  CHECK_PRINTF ("   0012345", "%10.7w16d", (int16_t) 12345);
  CHECK_PRINTF ("   0012345", "%*.7w16d", 10, (int16_t) 12345);
  CHECK_PRINTF ("   0012345", "%10.*w16d", 7, (int16_t) 12345);
  CHECK_PRINTF ("   0012345", "%*.*w16d", 10, 7, (int16_t) 12345);
  CHECK_PRINTF ("0012345   ", "%-10.7w16d", (int16_t) 12345);
  CHECK_PRINTF ("0012345   ", "%-*.7w16d", 10, (int16_t) 12345);
  CHECK_PRINTF ("0012345   ", "%-10.*w16d", 7, (int16_t) 12345);
  CHECK_PRINTF ("0012345   ", "%-*.*w16d", 10, 7, (int16_t) 12345);
  {
    int16_t n = -1;
    CHECK_PRINTF ("12345", "%d%w16n", 12345, &n);
    TEST_COMPARE (n, 5);
  }
  CHECK_PRINTF ("12345", "%w16d", (int_least16_t) 12345);
  CHECK_PRINTF ("-12345", "%w16d", (int_least16_t) -12345);
  CHECK_PRINTF ("12345", "%w16i", (int_least16_t) 12345);
  CHECK_PRINTF ("-12345", "%w16i", (int_least16_t) -12345);
  CHECK_PRINTF ("11000000111001", "%w16b", (uint_least16_t) 12345);
  CHECK_PRINTF ("11000000111001", "%w16B", (uint_least16_t) 12345);
  CHECK_PRINTF ("30071", "%w16o", (uint_least16_t) 12345);
  CHECK_PRINTF ("12345", "%w16u", (uint_least16_t) 12345);
  CHECK_PRINTF ("303a", "%w16x", (uint_least16_t) 12346);
  CHECK_PRINTF ("303A", "%w16X", (uint_least16_t) 12346);
  CHECK_PRINTF ("  12345", "%7w16d", (int_least16_t) 12345);
  CHECK_PRINTF ("  12345", "%*w16d", 7, (int_least16_t) 12345);
  CHECK_PRINTF ("0x3039", "%#w16x", (uint_least16_t) 12345);
  CHECK_PRINTF ("0012345", "%.7w16d", (int_least16_t) 12345);
  CHECK_PRINTF ("0012345", "%.*w16d", 7, (int_least16_t) 12345);
  CHECK_PRINTF ("   0012345", "%10.7w16d", (int_least16_t) 12345);
  CHECK_PRINTF ("   0012345", "%*.7w16d", 10, (int_least16_t) 12345);
  CHECK_PRINTF ("   0012345", "%10.*w16d", 7, (int_least16_t) 12345);
  CHECK_PRINTF ("   0012345", "%*.*w16d", 10, 7, (int_least16_t) 12345);
  CHECK_PRINTF ("0012345   ", "%-10.7w16d", (int_least16_t) 12345);
  CHECK_PRINTF ("0012345   ", "%-*.7w16d", 10, (int_least16_t) 12345);
  CHECK_PRINTF ("0012345   ", "%-10.*w16d", 7, (int_least16_t) 12345);
  CHECK_PRINTF ("0012345   ", "%-*.*w16d", 10, 7, (int_least16_t) 12345);
  {
    int_least16_t ln = -1;
    CHECK_PRINTF ("12345", "%d%w16n", 12345, &ln);
    TEST_COMPARE (ln, 5);
  }
  /* Test truncation of value in promoted type not representable in
     narrower type.  */
  CHECK_PRINTF ("4464", "%w16d", 70000);
  CHECK_PRINTF ("-4464", "%w16d", -70000);
  CHECK_PRINTF ("-7616", "%w16d", 123456);
  CHECK_PRINTF ("7616", "%w16d", -123456);
  CHECK_PRINTF ("57920", "%w16u", 123456);
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 10 test2 20", "%4$s %3$w16d %2$s %1$w16d",
		65556, "test2", 65546, "test");
}

static void
test_wf16 (void)
{
  CHAR buf[1024];
  _Static_assert (sizeof (int_fast16_t) == sizeof (long int),
		  "test assumes size of int_fast16_t");
  CHECK_PRINTF ("1234567", "%wf16d", (int_fast16_t) 1234567);
  CHECK_PRINTF ("-1234567", "%wf16d", (int_fast16_t) -1234567);
  CHECK_PRINTF ("1234567", "%wf16i", (int_fast16_t) 1234567);
  CHECK_PRINTF ("-1234567", "%wf16i", (int_fast16_t) -1234567);
  CHECK_PRINTF ("100101101011010000111", "%wf16b", (uint_fast16_t) 1234567);
  CHECK_PRINTF ("100101101011010000111", "%wf16B", (uint_fast16_t) 1234567);
  CHECK_PRINTF ("4553207", "%wf16o", (uint_fast16_t) 1234567);
  CHECK_PRINTF ("1234567", "%wf16u", (uint_fast16_t) 1234567);
  CHECK_PRINTF ("12d687", "%wf16x", (uint_fast16_t) 1234567);
  CHECK_PRINTF ("12D687", "%wf16X", (uint_fast16_t) 1234567);
  CHECK_PRINTF ("  1234567", "%9wf16d", (int_fast16_t) 1234567);
  CHECK_PRINTF ("  1234567", "%*wf16d", 9, (int_fast16_t) 1234567);
  CHECK_PRINTF ("0x12d687", "%#wf16x", (uint_fast16_t) 1234567);
  CHECK_PRINTF ("001234567", "%.9wf16d", (int_fast16_t) 1234567);
  CHECK_PRINTF ("001234567", "%.*wf16d", 9, (int_fast16_t) 1234567);
  CHECK_PRINTF ("   001234567", "%12.9wf16d", (int_fast16_t) 1234567);
  CHECK_PRINTF ("   001234567", "%*.9wf16d", 12, (int_fast16_t) 1234567);
  CHECK_PRINTF ("   001234567", "%12.*wf16d", 9, (int_fast16_t) 1234567);
  CHECK_PRINTF ("   001234567", "%*.*wf16d", 12, 9, (int_fast16_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-12.9wf16d", (int_fast16_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-*.9wf16d", 12, (int_fast16_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-12.*wf16d", 9, (int_fast16_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-*.*wf16d", 12, 9, (int_fast16_t) 1234567);
  {
    int_fast16_t n = -1;
    CHECK_PRINTF ("12345", "%d%wf16n", 12345, &n);
    TEST_COMPARE (n, 5);
  }
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 123456 test2 234567", "%4$s %3$wf16d %2$s %1$wf16d",
		(int_fast16_t) 234567, "test2", (int_fast16_t) 123456, "test");
#if INT_FAST16_MAX > 0x7fffffff
  CHECK_PRINTF ("12345678901", "%wf16d", (int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%wf16d", (int_fast16_t) -12345678901LL);
  CHECK_PRINTF ("12345678901", "%wf16i", (int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%wf16i", (int_fast16_t) -12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%wf16b",
		(uint_fast16_t) 12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%wf16B",
		(uint_fast16_t) 12345678901LL);
  CHECK_PRINTF ("133767016065", "%wf16o", (uint_fast16_t) 12345678901LL);
  CHECK_PRINTF ("12345678901", "%wf16u", (uint_fast16_t) 12345678901LL);
  CHECK_PRINTF ("2dfdc1c35", "%wf16x", (uint_fast16_t) 12345678901LL);
  CHECK_PRINTF ("2DFDC1C35", "%wf16X", (uint_fast16_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%13wf16d", (int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%*wf16d", 13, (int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("0x2dfdc1c35", "%#wf16x", (uint_fast16_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.13wf16d", (int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.*wf16d", 13, (int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.13wf16d",
		(int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.13wf16d", 16,
		(int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.*wf16d", 13,
		(int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.*wf16d", 16, 13,
		(int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.13wf16d",
		(int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.13wf16d", 16,
		(int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.*wf16d", 13,
		(int_fast16_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.*wf16d", 16, 13,
		(int_fast16_t) 12345678901LL);
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 123456789012 test2 234567890123",
		"%4$s %3$wf16d %2$s %1$wf16d",
		(int_fast16_t) 234567890123ULL, "test2",
		(int_fast16_t) 123456789012ULL, "test");
#endif
}

static void
test_w32 (void)
{
  CHAR buf[1024];
  CHECK_PRINTF ("1234567", "%w32d", (int32_t) 1234567);
  CHECK_PRINTF ("-1234567", "%w32d", (int32_t) -1234567);
  CHECK_PRINTF ("1234567", "%w32i", (int32_t) 1234567);
  CHECK_PRINTF ("-1234567", "%w32i", (int32_t) -1234567);
  CHECK_PRINTF ("100101101011010000111", "%w32b", (uint32_t) 1234567);
  CHECK_PRINTF ("100101101011010000111", "%w32B", (uint32_t) 1234567);
  CHECK_PRINTF ("4553207", "%w32o", (uint32_t) 1234567);
  CHECK_PRINTF ("1234567", "%w32u", (uint32_t) 1234567);
  CHECK_PRINTF ("12d687", "%w32x", (uint32_t) 1234567);
  CHECK_PRINTF ("12D687", "%w32X", (uint32_t) 1234567);
  CHECK_PRINTF ("  1234567", "%9w32d", (int32_t) 1234567);
  CHECK_PRINTF ("  1234567", "%*w32d", 9, (int32_t) 1234567);
  CHECK_PRINTF ("0x12d687", "%#w32x", (uint32_t) 1234567);
  CHECK_PRINTF ("001234567", "%.9w32d", (int32_t) 1234567);
  CHECK_PRINTF ("001234567", "%.*w32d", 9, (int32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%12.9w32d", (int32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%*.9w32d", 12, (int32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%12.*w32d", 9, (int32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%*.*w32d", 12, 9, (int32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-12.9w32d", (int32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-*.9w32d", 12, (int32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-12.*w32d", 9, (int32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-*.*w32d", 12, 9, (int32_t) 1234567);
  {
    int32_t n = -1;
    CHECK_PRINTF ("12345", "%d%w32n", 12345, &n);
    TEST_COMPARE (n, 5);
  }
  CHECK_PRINTF ("1234567", "%w32d", (int_least32_t) 1234567);
  CHECK_PRINTF ("-1234567", "%w32d", (int_least32_t) -1234567);
  CHECK_PRINTF ("1234567", "%w32i", (int_least32_t) 1234567);
  CHECK_PRINTF ("-1234567", "%w32i", (int_least32_t) -1234567);
  CHECK_PRINTF ("100101101011010000111", "%w32b", (uint_least32_t) 1234567);
  CHECK_PRINTF ("100101101011010000111", "%w32B", (uint_least32_t) 1234567);
  CHECK_PRINTF ("4553207", "%w32o", (uint_least32_t) 1234567);
  CHECK_PRINTF ("1234567", "%w32u", (uint_least32_t) 1234567);
  CHECK_PRINTF ("12d687", "%w32x", (uint_least32_t) 1234567);
  CHECK_PRINTF ("12D687", "%w32X", (uint_least32_t) 1234567);
  CHECK_PRINTF ("  1234567", "%9w32d", (int_least32_t) 1234567);
  CHECK_PRINTF ("  1234567", "%*w32d", 9, (int_least32_t) 1234567);
  CHECK_PRINTF ("0x12d687", "%#w32x", (uint_least32_t) 1234567);
  CHECK_PRINTF ("001234567", "%.9w32d", (int_least32_t) 1234567);
  CHECK_PRINTF ("001234567", "%.*w32d", 9, (int_least32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%12.9w32d", (int_least32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%*.9w32d", 12, (int_least32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%12.*w32d", 9, (int_least32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%*.*w32d", 12, 9, (int_least32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-12.9w32d", (int_least32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-*.9w32d", 12, (int_least32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-12.*w32d", 9, (int_least32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-*.*w32d", 12, 9, (int_least32_t) 1234567);
  {
    int_least32_t ln = -1;
    CHECK_PRINTF ("12345", "%d%w32n", 12345, &ln);
    TEST_COMPARE (ln, 5);
  }
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 123456 test2 234567", "%4$s %3$w32d %2$s %1$w32d",
		INT32_C (234567), "test2", INT32_C (123456), "test");
}

static void
test_wf32 (void)
{
  CHAR buf[1024];
  _Static_assert (sizeof (int_fast32_t) == sizeof (long int),
		  "test assumes size of int_fast32_t");
  CHECK_PRINTF ("1234567", "%wf32d", (int_fast32_t) 1234567);
  CHECK_PRINTF ("-1234567", "%wf32d", (int_fast32_t) -1234567);
  CHECK_PRINTF ("1234567", "%wf32i", (int_fast32_t) 1234567);
  CHECK_PRINTF ("-1234567", "%wf32i", (int_fast32_t) -1234567);
  CHECK_PRINTF ("100101101011010000111", "%wf32b", (uint_fast32_t) 1234567);
  CHECK_PRINTF ("100101101011010000111", "%wf32B", (uint_fast32_t) 1234567);
  CHECK_PRINTF ("4553207", "%wf32o", (uint_fast32_t) 1234567);
  CHECK_PRINTF ("1234567", "%wf32u", (uint_fast32_t) 1234567);
  CHECK_PRINTF ("12d687", "%wf32x", (uint_fast32_t) 1234567);
  CHECK_PRINTF ("12D687", "%wf32X", (uint_fast32_t) 1234567);
  CHECK_PRINTF ("  1234567", "%9wf32d", (int_fast32_t) 1234567);
  CHECK_PRINTF ("  1234567", "%*wf32d", 9, (int_fast32_t) 1234567);
  CHECK_PRINTF ("0x12d687", "%#wf32x", (uint_fast32_t) 1234567);
  CHECK_PRINTF ("001234567", "%.9wf32d", (int_fast32_t) 1234567);
  CHECK_PRINTF ("001234567", "%.*wf32d", 9, (int_fast32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%12.9wf32d", (int_fast32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%*.9wf32d", 12, (int_fast32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%12.*wf32d", 9, (int_fast32_t) 1234567);
  CHECK_PRINTF ("   001234567", "%*.*wf32d", 12, 9, (int_fast32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-12.9wf32d", (int_fast32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-*.9wf32d", 12, (int_fast32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-12.*wf32d", 9, (int_fast32_t) 1234567);
  CHECK_PRINTF ("001234567   ", "%-*.*wf32d", 12, 9, (int_fast32_t) 1234567);
  {
    int_fast32_t n = -1;
    CHECK_PRINTF ("12345", "%d%wf32n", 12345, &n);
    TEST_COMPARE (n, 5);
  }
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 123456 test2 234567", "%4$s %3$wf32d %2$s %1$wf32d",
		(int_fast32_t) 234567, "test2", (int_fast32_t) 123456, "test");
#if INT_FAST32_MAX > 0x7fffffff
  CHECK_PRINTF ("12345678901", "%wf32d", (int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%wf32d", (int_fast32_t) -12345678901LL);
  CHECK_PRINTF ("12345678901", "%wf32i", (int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%wf32i", (int_fast32_t) -12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%wf32b",
		(uint_fast32_t) 12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%wf32B",
		(uint_fast32_t) 12345678901LL);
  CHECK_PRINTF ("133767016065", "%wf32o", (uint_fast32_t) 12345678901LL);
  CHECK_PRINTF ("12345678901", "%wf32u", (uint_fast32_t) 12345678901LL);
  CHECK_PRINTF ("2dfdc1c35", "%wf32x", (uint_fast32_t) 12345678901LL);
  CHECK_PRINTF ("2DFDC1C35", "%wf32X", (uint_fast32_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%13wf32d", (int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%*wf32d", 13, (int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("0x2dfdc1c35", "%#wf32x", (uint_fast32_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.13wf32d", (int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.*wf32d", 13, (int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.13wf32d",
		(int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.13wf32d", 16,
		(int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.*wf32d", 13,
		(int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.*wf32d", 16, 13,
		(int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.13wf32d",
		(int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.13wf32d", 16,
		(int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.*wf32d", 13,
		(int_fast32_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.*wf32d", 16, 13,
		(int_fast32_t) 12345678901LL);
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 123456789012 test2 234567890123",
		"%4$s %3$wf32d %2$s %1$wf32d",
		(int_fast32_t) 234567890123ULL, "test2",
		(int_fast32_t) 123456789012ULL, "test");
#endif
}

static void
test_w64 (void)
{
  CHAR buf[1024];
  CHECK_PRINTF ("12345678901", "%w64d", (int64_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%w64d", (int64_t) -12345678901LL);
  CHECK_PRINTF ("12345678901", "%w64i", (int64_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%w64i", (int64_t) -12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%w64b",
		(uint64_t) 12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%w64B",
		(uint64_t) 12345678901LL);
  CHECK_PRINTF ("133767016065", "%w64o", (uint64_t) 12345678901LL);
  CHECK_PRINTF ("12345678901", "%w64u", (uint64_t) 12345678901LL);
  CHECK_PRINTF ("2dfdc1c35", "%w64x", (uint64_t) 12345678901LL);
  CHECK_PRINTF ("2DFDC1C35", "%w64X", (uint64_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%13w64d", (int64_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%*w64d", 13, (int64_t) 12345678901LL);
  CHECK_PRINTF ("0x2dfdc1c35", "%#w64x", (uint64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.13w64d", (int64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.*w64d", 13, (int64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.13w64d", (int64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.13w64d", 16, (int64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.*w64d", 13, (int64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.*w64d", 16, 13,
		(int64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.13w64d", (int64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.13w64d", 16, (int64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.*w64d", 13, (int64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.*w64d", 16, 13,
		(int64_t) 12345678901LL);
  {
    int64_t n = -1;
    CHECK_PRINTF ("12345", "%d%w64n", 12345, &n);
    TEST_COMPARE (n, 5);
  }
  CHECK_PRINTF ("12345678901", "%w64d", (int_least64_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%w64d", (int_least64_t) -12345678901LL);
  CHECK_PRINTF ("12345678901", "%w64i", (int_least64_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%w64i", (int_least64_t) -12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%w64b",
		(uint_least64_t) 12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%w64B",
		(uint_least64_t) 12345678901LL);
  CHECK_PRINTF ("133767016065", "%w64o", (uint_least64_t) 12345678901LL);
  CHECK_PRINTF ("12345678901", "%w64u", (uint_least64_t) 12345678901LL);
  CHECK_PRINTF ("2dfdc1c35", "%w64x", (uint_least64_t) 12345678901LL);
  CHECK_PRINTF ("2DFDC1C35", "%w64X", (uint_least64_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%13w64d", (int_least64_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%*w64d", 13, (int_least64_t) 12345678901LL);
  CHECK_PRINTF ("0x2dfdc1c35", "%#w64x", (uint_least64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.13w64d", (int_least64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.*w64d", 13, (int_least64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.13w64d",
		(int_least64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.13w64d", 16,
		(int_least64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.*w64d", 13,
		(int_least64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.*w64d", 16, 13,
		(int_least64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.13w64d",
		(int_least64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.13w64d", 16,
		(int_least64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.*w64d", 13,
		(int_least64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.*w64d", 16, 13,
		(int_least64_t) 12345678901LL);
  {
    int_least64_t ln = -1;
    CHECK_PRINTF ("12345", "%d%w64n", 12345, &ln);
    TEST_COMPARE (ln, 5);
  }
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 123456789012 test2 234567890123",
		"%4$s %3$w64d %2$s %1$w64d",
		INT64_C (234567890123), "test2",
		INT64_C (123456789012), "test");
}

static void
test_wf64 (void)
{
  CHAR buf[1024];
  _Static_assert (sizeof (int_fast64_t) == sizeof (long long int),
		  "test assumes size of int_fast64_t");
  CHECK_PRINTF ("12345678901", "%wf64d", (int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%wf64d", (int_fast64_t) -12345678901LL);
  CHECK_PRINTF ("12345678901", "%wf64i", (int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("-12345678901", "%wf64i", (int_fast64_t) -12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%wf64b",
		(uint_fast64_t) 12345678901LL);
  CHECK_PRINTF ("1011011111110111000001110000110101", "%wf64B",
		(uint_fast64_t) 12345678901LL);
  CHECK_PRINTF ("133767016065", "%wf64o", (uint_fast64_t) 12345678901LL);
  CHECK_PRINTF ("12345678901", "%wf64u", (uint_fast64_t) 12345678901LL);
  CHECK_PRINTF ("2dfdc1c35", "%wf64x", (uint_fast64_t) 12345678901LL);
  CHECK_PRINTF ("2DFDC1C35", "%wf64X", (uint_fast64_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%13wf64d", (int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("  12345678901", "%*wf64d", 13, (int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("0x2dfdc1c35", "%#wf64x", (uint_fast64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.13wf64d", (int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901", "%.*wf64d", 13, (int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.13wf64d",
		(int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.13wf64d", 16,
		(int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%16.*wf64d", 13,
		(int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("   0012345678901", "%*.*wf64d", 16, 13,
		(int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.13wf64d",
		(int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.13wf64d", 16,
		(int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-16.*wf64d", 13,
		(int_fast64_t) 12345678901LL);
  CHECK_PRINTF ("0012345678901   ", "%-*.*wf64d", 16, 13,
		(int_fast64_t) 12345678901LL);
  {
    int_fast64_t n = -1;
    CHECK_PRINTF ("12345", "%d%wf64n", 12345, &n);
    TEST_COMPARE (n, 5);
  }
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 123456789012 test2 234567890123",
		"%4$s %3$wf64d %2$s %1$wf64d",
		(int_fast64_t) 234567890123ULL, "test2",
		(int_fast64_t) 123456789012ULL, "test");
}

static int
do_test (void)
{
  test_w8 ();
  test_wf8 ();
  test_w16 ();
  test_wf16 ();
  test_w32 ();
  test_wf32 ();
  test_w64 ();
  test_wf64 ();
  /* Bad N in %wN and %wfN are required to produce an error return
     from printf functions (and can also be seen to be invalid at
     compile time).  */
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (13, "-Wformat");
  DIAG_IGNORE_NEEDS_COMMENT (13, "-Wformat-extra-args");
  CHAR buf[1024];
  CHECK_PRINTF_ERR ("%w1d", 123);
  CHECK_PRINTF_ERR ("%w123d", 123);
  CHECK_PRINTF_ERR ("%w99999999999999999999d", 123);
  CHECK_PRINTF_ERR ("%wf1d", 123);
  CHECK_PRINTF_ERR ("%wf123d", 123);
  CHECK_PRINTF_ERR ("%wf99999999999999999999d", 123);
  CHECK_PRINTF_ERR ("%1$w1d", 123);
  CHECK_PRINTF_ERR ("%1$w123d", 123);
  CHECK_PRINTF_ERR ("%1$w99999999999999999999d", 123);
  CHECK_PRINTF_ERR ("%1$wf1d", 123);
  CHECK_PRINTF_ERR ("%1$wf123d", 123);
  CHECK_PRINTF_ERR ("%1$wf99999999999999999999d", 123);
  DIAG_POP_NEEDS_COMMENT;
  return 0;
}

DIAG_POP_NEEDS_COMMENT;

#include <support/test-driver.c>
