/* Test c8rtomb.
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

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>
#include <support/check.h>
#include <support/support.h>

static int
test_truncated_code_unit_sequence (void)
{
  /* Missing trailing code unit for a two code byte unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xC2";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Missing first trailing code unit for a three byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xE0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Missing second trailing code unit for a three byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xE0\xA0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Missing first trailing code unit for a four byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Missing second trailing code unit for a four byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF0\x90";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Missing third trailing code unit for a four byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF0\x90\x80";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t)  0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[3], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  return 0;
}

static int
test_invalid_trailing_code_unit_sequence (void)
{
  /* Invalid trailing code unit for a two code byte unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xC2\xC0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Invalid first trailing code unit for a three byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xE0\xC0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Invalid second trailing code unit for a three byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xE0\xA0\xC0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Invalid first trailing code unit for a four byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF0\xC0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Invalid second trailing code unit for a four byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF0\x90\xC0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Invalid third trailing code unit for a four byte code unit sequence.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF0\x90\x80\xC0";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t)  0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[3], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  return 0;
}

static int
test_lone_trailing_code_units (void)
{
  /* Lone trailing code unit.  */
  const char8_t *u8s = (const char8_t*) u8"\x80";
  char buf[MB_LEN_MAX] = { 0 };
  mbstate_t s = { 0 };

  errno = 0;
  TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) -1);
  TEST_COMPARE (errno, EILSEQ);

  return 0;
}

static int
test_overlong_encoding (void)
{
  /* Two byte overlong encoding.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xC0\x80";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Two byte overlong encoding.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xC1\x80";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Three byte overlong encoding.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xE0\x9F\xBF";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Four byte overlong encoding.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF0\x8F\xBF\xBF";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  return 0;
}

static int
test_surrogate_range (void)
{
  /* Would encode U+D800.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xED\xA0\x80";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Would encode U+DFFF.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xED\xBF\xBF";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  return 0;
}

static int
test_out_of_range_encoding (void)
{
  /* Would encode U+00110000.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF4\x90\x80\x80";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  /* Would encode U+00140000.  */
  {
    const char8_t *u8s = (const char8_t*) u8"\xF5\x90\x80\x80";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    errno = 0;
    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  return 0;
}

static int
test_null_output_buffer (void)
{
  /* Null character with an initial state.  */
  {
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (NULL, u8"X"[0], &s), (size_t) 1);
    /* Assert the state is now an initial state.  */
    TEST_VERIFY (mbsinit (&s));
  }

  /* Null buffer with a state corresponding to an incompletely read code
     unit sequence.  In this case, an error occurs since insufficient
     information is available to complete the already started code unit
     sequence and return to the initial state.  */
  {
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8"\xC2"[0], &s), (size_t)  0);
    errno = 0;
    TEST_COMPARE (c8rtomb (NULL, u8"\x80"[0], &s), (size_t) -1);
    TEST_COMPARE (errno, EILSEQ);
  }

  return 0;
}

static int
test_utf8 (void)
{
  xsetlocale (LC_ALL, "de_DE.UTF-8");

  /* Null character.  */
  {
    /* U+0000 => 0x00 */
    const char8_t *u8s = (const char8_t*) u8"\x00";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 1);
    TEST_COMPARE (buf[0], (char) 0x00);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First non-null character in the code point range that maps to a single
     code unit.  */
  {
    /* U+0001 => 0x01 */
    const char8_t *u8s = (const char8_t*) u8"\x01";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 1);
    TEST_COMPARE (buf[0], (char) 0x01);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to a single code unit.  */
  {
    /* U+007F => 0x7F */
    const char8_t *u8s = (const char8_t*) u8"\x7F";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 1);
    TEST_COMPARE (buf[0], (char) 0x7F);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First character in the code point range that maps to two code units.  */
  {
    /* U+0080 => 0xC2 0x80 */
    const char8_t *u8s = (const char8_t*) u8"\xC2\x80";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 2);
    TEST_COMPARE (buf[0], (char) 0xC2);
    TEST_COMPARE (buf[1], (char) 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to two code units.  */
  {
    /* U+07FF => 0xDF 0xBF */
    const char8_t *u8s = (const char8_t*) u8"\u07FF";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 2);
    TEST_COMPARE (buf[0], (char) 0xDF);
    TEST_COMPARE (buf[1], (char) 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First character in the code point range that maps to three code units.  */
  {
    /* U+0800 => 0xE0 0xA0 0x80 */
    const char8_t *u8s = (const char8_t*) u8"\u0800";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 3);
    TEST_COMPARE (buf[0], (char) 0xE0);
    TEST_COMPARE (buf[1], (char) 0xA0);
    TEST_COMPARE (buf[2], (char) 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to three code units
     before the surrogate code point range.  */
  {
    /* U+D7FF => 0xED 0x9F 0xBF */
    const char8_t *u8s = (const char8_t*) u8"\uD7FF";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 3);
    TEST_COMPARE (buf[0], (char) 0xED);
    TEST_COMPARE (buf[1], (char) 0x9F);
    TEST_COMPARE (buf[2], (char) 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First character in the code point range that maps to three code units
     after the surrogate code point range.  */
  {
    /* U+E000 => 0xEE 0x80 0x80 */
    const char8_t *u8s = (const char8_t*) u8"\uE000";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 3);
    TEST_COMPARE (buf[0], (char) 0xEE);
    TEST_COMPARE (buf[1], (char) 0x80);
    TEST_COMPARE (buf[2], (char) 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Not a BOM.  */
  {
    /* U+FEFF => 0xEF 0xBB 0xBF */
    const char8_t *u8s = (const char8_t*) u8"\uFEFF";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 3);
    TEST_COMPARE (buf[0], (char) 0xEF);
    TEST_COMPARE (buf[1], (char) 0xBB);
    TEST_COMPARE (buf[2], (char) 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Replacement character.  */
  {
    /* U+FFFD => 0xEF 0xBF 0xBD */
    const char8_t *u8s = (const char8_t*) u8"\uFFFD";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 3);
    TEST_COMPARE (buf[0], (char) 0xEF);
    TEST_COMPARE (buf[1], (char) 0xBF);
    TEST_COMPARE (buf[2], (char) 0xBD);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to three code units.  */
  {
    /* U+FFFF => 0xEF 0xBF 0xBF */
    const char8_t *u8s = (const char8_t*) u8"\uFFFF";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 3);
    TEST_COMPARE (buf[0], (char) 0xEF);
    TEST_COMPARE (buf[1], (char) 0xBF);
    TEST_COMPARE (buf[2], (char) 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First character in the code point range that maps to four code units.  */
  {
    /* U+10000 => 0xF0 0x90 0x80 0x80 */
    const char8_t *u8s = (const char8_t*) u8"\U00010000";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[3], &s), (size_t) 4);
    TEST_COMPARE (buf[0], (char) 0xF0);
    TEST_COMPARE (buf[1], (char) 0x90);
    TEST_COMPARE (buf[2], (char) 0x80);
    TEST_COMPARE (buf[3], (char) 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to four code units.  */
  {
    /* U+10FFFF => 0xF4 0x8F 0xBF 0xBF */
    const char8_t *u8s = (const char8_t*) u8"\U0010FFFF";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[3], &s), (size_t) 4);
    TEST_COMPARE (buf[0], (char) 0xF4);
    TEST_COMPARE (buf[1], (char) 0x8F);
    TEST_COMPARE (buf[2], (char) 0xBF);
    TEST_COMPARE (buf[3], (char) 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  return 0;
}

static int
test_big5_hkscs (void)
{
  xsetlocale (LC_ALL, "zh_HK.BIG5-HKSCS");

  /* A pair of two byte UTF-8 code unit sequences that map a Unicode code
     point and combining character to a single double byte character.  */
  {
    /* U+00CA U+0304 => 0x88 0x62 */
    const char8_t *u8s = (const char8_t*) u8"\u00CA\u0304";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[3], &s), (size_t) 2);
    TEST_COMPARE (buf[0], (char) 0x88);
    TEST_COMPARE (buf[1], (char) 0x62);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Another pair of two byte UTF-8 code unit sequences that map a Unicode code
     point and combining character to a single double byte character.  */
  {
    /* U+00EA U+030C => 0x88 0xA5 */
    const char8_t *u8s = (const char8_t*) u8"\u00EA\u030C";
    char buf[MB_LEN_MAX] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (c8rtomb (buf, u8s[0], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[1], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[2], &s), (size_t) 0);
    TEST_COMPARE (c8rtomb (buf, u8s[3], &s), (size_t) 2);
    TEST_COMPARE (buf[0], (char) 0x88);
    TEST_COMPARE (buf[1], (char) 0xA5);
    TEST_VERIFY (mbsinit (&s));
  }

  return 0;
}

static int
do_test (void)
{
  test_truncated_code_unit_sequence ();
  test_invalid_trailing_code_unit_sequence ();
  test_lone_trailing_code_units ();
  test_overlong_encoding ();
  test_surrogate_range ();
  test_out_of_range_encoding ();
  test_null_output_buffer ();
  test_utf8 ();
  test_big5_hkscs ();
  return 0;
}

#include <support/test-driver.c>
