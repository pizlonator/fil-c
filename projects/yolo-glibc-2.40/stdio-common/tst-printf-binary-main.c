/* Test binary printf formats.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <libc-diag.h>
#include <support/check.h>

/* GCC does not know the %b or %B formats before GCC 12.  */
DIAG_PUSH_NEEDS_COMMENT;
#if !__GNUC_PREREQ (12, 0)
DIAG_IGNORE_NEEDS_COMMENT (11, "-Wformat");
DIAG_IGNORE_NEEDS_COMMENT (11, "-Wformat-extra-args");
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

static int
do_test (void)
{
  CHAR buf[1024];
  CHECK_PRINTF ("0", "%b", 0u);
  CHECK_PRINTF ("0", "%B", 0u);
  CHECK_PRINTF ("0", "%#b", 0u);
  CHECK_PRINTF ("0", "%#B", 0u);
  CHECK_PRINTF ("1", "%b", 1u);
  CHECK_PRINTF ("1", "%B", 1u);
  CHECK_PRINTF ("10", "%b", 2u);
  CHECK_PRINTF ("10", "%B", 2u);
  CHECK_PRINTF ("11", "%b", 3u);
  CHECK_PRINTF ("11", "%B", 3u);
  CHECK_PRINTF ("10000111011001010100001100100001", "%b", 0x87654321);
  CHECK_PRINTF ("10000111011001010100001100100001", "%B", 0x87654321);
  CHECK_PRINTF ("100001100100001", "%hb", (int) 0x87654321);
  CHECK_PRINTF ("100001100100001", "%hB", (int) 0x87654321);
  CHECK_PRINTF ("100001", "%hhb", (int) 0x87654321);
  CHECK_PRINTF ("100001", "%hhB", (int) 0x87654321);
  CHECK_PRINTF ("10000111011001010100001100100001", "%lb", 0x87654321ul);
  CHECK_PRINTF ("10000111011001010100001100100001", "%lB", 0x87654321ul);
  CHECK_PRINTF ("11111110110111001011101010011001"
		"10000111011001010100001100100001", "%llb",
		0xfedcba9987654321ull);
  CHECK_PRINTF ("11111110110111001011101010011001"
		"10000111011001010100001100100001", "%llB",
		0xfedcba9987654321ull);
#if LONG_WIDTH >= 64
  CHECK_PRINTF ("11111110110111001011101010011001"
		"10000111011001010100001100100001", "%lb",
		0xfedcba9987654321ul);
  CHECK_PRINTF ("11111110110111001011101010011001"
		"10000111011001010100001100100001", "%lB",
		0xfedcba9987654321ul);
#endif
  CHECK_PRINTF ("0b11", "%#" PRIb8, (uint8_t) 3);
  CHECK_PRINTF ("0b11", "%#" PRIb16, (uint16_t) 3);
  CHECK_PRINTF ("0b10000111011001010100001100100001", "%#" PRIb32,
		(uint32_t) 0x87654321);
  CHECK_PRINTF ("0b11111110110111001011101010011001"
		"10000111011001010100001100100001", "%#" PRIb64,
		(uint64_t) 0xfedcba9987654321ull);
  CHECK_PRINTF ("0b11", "%#" PRIbLEAST8, (uint_least8_t) 3);
  CHECK_PRINTF ("0b11", "%#" PRIbLEAST16, (uint_least16_t) 3);
  CHECK_PRINTF ("0b10000111011001010100001100100001", "%#" PRIbLEAST32,
		(uint_least32_t) 0x87654321);
  CHECK_PRINTF ("0b11111110110111001011101010011001"
		"10000111011001010100001100100001", "%#" PRIbLEAST64,
		(uint_least64_t) 0xfedcba9987654321ull);
  CHECK_PRINTF ("0b11", "%#" PRIbFAST8, (uint_fast8_t) 3);
  CHECK_PRINTF ("0b11", "%#" PRIbFAST16, (uint_fast16_t) 3);
  CHECK_PRINTF ("0b10000111011001010100001100100001", "%#" PRIbFAST32,
		(uint_fast32_t) 0x87654321);
  CHECK_PRINTF ("0b11111110110111001011101010011001"
		"10000111011001010100001100100001", "%#" PRIbFAST64,
		(uint_fast64_t) 0xfedcba9987654321ull);
  CHECK_PRINTF ("0b10000111011001010100001100100001", "%#" PRIbPTR,
		(uintptr_t) 0x87654321);
  CHECK_PRINTF ("0b11111110110111001011101010011001"
		"10000111011001010100001100100001", "%#" PRIbMAX,
		(uintmax_t) 0xfedcba9987654321ull);
  CHECK_PRINTF ("0B11", "%#" PRIB8, (uint8_t) 3);
  CHECK_PRINTF ("0B11", "%#" PRIB16, (uint16_t) 3);
  CHECK_PRINTF ("0B10000111011001010100001100100001", "%#" PRIB32,
		(uint32_t) 0x87654321);
  CHECK_PRINTF ("0B11111110110111001011101010011001"
		"10000111011001010100001100100001", "%#" PRIB64,
		(uint64_t) 0xfedcba9987654321ull);
  CHECK_PRINTF ("0B11", "%#" PRIBLEAST8, (uint_least8_t) 3);
  CHECK_PRINTF ("0B11", "%#" PRIBLEAST16, (uint_least16_t) 3);
  CHECK_PRINTF ("0B10000111011001010100001100100001", "%#" PRIBLEAST32,
		(uint_least32_t) 0x87654321);
  CHECK_PRINTF ("0B11111110110111001011101010011001"
		"10000111011001010100001100100001", "%#" PRIBLEAST64,
		(uint_least64_t) 0xfedcba9987654321ull);
  CHECK_PRINTF ("0B11", "%#" PRIBFAST8, (uint_fast8_t) 3);
  CHECK_PRINTF ("0B11", "%#" PRIBFAST16, (uint_fast16_t) 3);
  CHECK_PRINTF ("0B10000111011001010100001100100001", "%#" PRIBFAST32,
		(uint_fast32_t) 0x87654321);
  CHECK_PRINTF ("0B11111110110111001011101010011001"
		"10000111011001010100001100100001", "%#" PRIBFAST64,
		(uint_fast64_t) 0xfedcba9987654321ull);
  CHECK_PRINTF ("0B10000111011001010100001100100001", "%#" PRIBPTR,
		(uintptr_t) 0x87654321);
  CHECK_PRINTF ("0B11111110110111001011101010011001"
		"10000111011001010100001100100001", "%#" PRIBMAX,
		(uintmax_t) 0xfedcba9987654321ull);
  CHECK_PRINTF (" 1010", "%5b", 10u);
  CHECK_PRINTF (" 1010", "%5B", 10u);
  CHECK_PRINTF ("01010", "%05b", 10u);
  CHECK_PRINTF ("01010", "%05B", 10u);
  CHECK_PRINTF ("1011 ", "%-5b", 11u);
  CHECK_PRINTF ("1011 ", "%-5B", 11u);
  CHECK_PRINTF ("0b10011", "%#b", 19u);
  CHECK_PRINTF ("0B10011", "%#B", 19u);
  CHECK_PRINTF ("   0b10011", "%#10b", 19u);
  CHECK_PRINTF ("   0B10011", "%#10B", 19u);
  CHECK_PRINTF ("0b00010011", "%0#10b", 19u);
  CHECK_PRINTF ("0B00010011", "%0#10B", 19u);
  CHECK_PRINTF ("0b00010011", "%#010b", 19u);
  CHECK_PRINTF ("0B00010011", "%#010B", 19u);
  CHECK_PRINTF ("0b10011   ", "%#-10b", 19u);
  CHECK_PRINTF ("0B10011   ", "%#-10B", 19u);
  CHECK_PRINTF ("00010011", "%.8b", 19u);
  CHECK_PRINTF ("00010011", "%.8B", 19u);
  CHECK_PRINTF ("0b00010011", "%#.8b", 19u);
  CHECK_PRINTF ("0B00010011", "%#.8B", 19u);
  CHECK_PRINTF ("       00010011", "%15.8b", 19u);
  CHECK_PRINTF ("       00010011", "%15.8B", 19u);
  CHECK_PRINTF ("00010011       ", "%-15.8b", 19u);
  CHECK_PRINTF ("00010011       ", "%-15.8B", 19u);
  CHECK_PRINTF ("     0b00010011", "%#15.8b", 19u);
  CHECK_PRINTF ("     0B00010011", "%#15.8B", 19u);
  CHECK_PRINTF ("0b00010011     ", "%-#15.8b", 19u);
  CHECK_PRINTF ("0B00010011     ", "%-#15.8B", 19u);
  /* GCC diagnoses ignored flags.  */
  DIAG_PUSH_NEEDS_COMMENT;
  DIAG_IGNORE_NEEDS_COMMENT (12, "-Wformat");
  /* '0' flag ignored with '-'.  */
  CHECK_PRINTF ("1011 ", "%0-5b", 11u);
  CHECK_PRINTF ("1011 ", "%0-5B", 11u);
  CHECK_PRINTF ("0b10011   ", "%#0-10b", 19u);
  CHECK_PRINTF ("0B10011   ", "%#0-10B", 19u);
  /* '0' flag ignored with precision.  */
  CHECK_PRINTF ("       00010011", "%015.8b", 19u);
  CHECK_PRINTF ("       00010011", "%015.8B", 19u);
  CHECK_PRINTF ("     0b00010011", "%0#15.8b", 19u);
  CHECK_PRINTF ("     0B00010011", "%0#15.8B", 19u);
  DIAG_POP_NEEDS_COMMENT;
  /* Test positional argument handling.  */
  CHECK_PRINTF ("test 1011 test2 100010001000100010001000100010001",
		"%2$s %1$b %4$s %3$llb", 11u, "test", 0x111111111ull, "test2");
  return 0;
}

DIAG_POP_NEEDS_COMMENT;

#include <support/test-driver.c>
