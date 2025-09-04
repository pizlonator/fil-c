/* Test mbrtoc8.
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

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>
#include <support/check.h>
#include <support/support.h>

static int
test_utf8 (void)
{
  xsetlocale (LC_ALL, "de_DE.UTF-8");

  /* No inputs.  */
  {
    const char *mbs = "";
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 0, &s), (size_t) -2); /* no input */
    TEST_VERIFY (mbsinit (&s));
  }

  /* Null character.  */
  {
    const char *mbs = "\x00"; /* 0x00 => U+0000 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 0);
    mbs += 1;
    TEST_COMPARE (buf[0], 0x00);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First non-null character in the code point range that maps to a single
     code unit.  */
  {
    const char *mbs = "\x01"; /* 0x01 => U+0001 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0x01);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to a single code unit.  */
  {
    const char *mbs = "\x7F"; /* 0x7F => U+007F */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0x7F);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First character in the code point range that maps to two code units.  */
  {
    const char *mbs = "\xC2\x80"; /* 0xC2 0x80 => U+0080 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 2);
    mbs += 2;
    TEST_COMPARE (buf[0], 0xC2);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xC2\x80"; /* 0xC2 0x80 => U+0080 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xC2);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to two code units.  */
  {
    const char *mbs = "\xDF\xBF"; /* 0xDF 0xBF => U+07FF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 2);
    mbs += 2;
    TEST_COMPARE (buf[0], 0xDF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xDF\xBF"; /* 0xDF 0xBF => U+07FF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xDF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First character in the code point range that maps to three code units.  */
  {
    const char *mbs = u8"\xE0\xA0\x80"; /* 0xE0 0xA0 0x80 => U+0800 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 3);
    mbs += 3;
    TEST_COMPARE (buf[0], 0xE0);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xA0);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = u8"\xE0\xA0\x80"; /* 0xE0 0xA0 0x80 => U+0800 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xE0);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xA0);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to three code units
     before the surrogate code point range.  */
  {
    const char *mbs = "\xED\x9F\xBF"; /* 0xED 0x9F 0xBF => U+D7FF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 3);
    mbs += 3;
    TEST_COMPARE (buf[0], 0xED);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x9F);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xED\x9F\xBF"; /* 0xED 0x9F 0xBF => U+D7FF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xED);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x9F);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First character in the code point range that maps to three code units
     after the surrogate code point range.  */
  {
    const char *mbs = "\xEE\x80\x80"; /* 0xEE 0x80 0x80 => U+E000 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 3);
    mbs += 3;
    TEST_COMPARE (buf[0], 0xEE);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xEE\x80\x80"; /* 0xEE 0x80 0x80 => U+E000 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xEE);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Not a BOM.  */
  {
    const char *mbs = "\xEF\xBB\xBF"; /* 0xEF 0xBB 0xBF => U+FEFF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 3);
    mbs += 3;
    TEST_COMPARE (buf[0], 0xEF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBB);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xEF\xBB\xBF"; /* 0xEF 0xBB 0xBF => U+FEFF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xEF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBB);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Replacement character.  */
  {
    const char *mbs = "\xEF\xBF\xBD"; /* 0xEF 0xBF 0xBD => U+FFFD */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 3);
    mbs += 3;
    TEST_COMPARE (buf[0], 0xEF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBD);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xEF\xBF\xBD"; /* 0xEF 0xBF 0xBD => U+FFFD */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xEF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBD);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to three code units.  */
  {
    const char *mbs = "\xEF\xBF\xBF"; /* 0xEF 0xBF 0xBF => U+FFFF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 3);
    mbs += 3;
    TEST_COMPARE (buf[0], 0xEF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xEF\xBF\xBF"; /* 0xEF 0xBF 0xBF => U+FFFF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xEF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* First character in the code point range that maps to four code units.  */
  {
    const char *mbs = "\xF0\x90\x80\x80"; /* 0xF0 0x90 0x80 0x80 => U+10000 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 4);
    mbs += 4;
    TEST_COMPARE (buf[0], 0xF0);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x90);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xF0\x90\x80\x80"; /* 0xF0 0x90 0x80 0x80 => U+10000 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xF0);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x90);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x80);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Last character in the code point range that maps to four code units.  */
  {
    const char *mbs = "\xF4\x8F\xBF\xBF"; /* 0xF4 0x8F 0xBF 0xBF => U+10FFFF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 4);
    mbs += 4;
    TEST_COMPARE (buf[0], 0xF4);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x8F);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\xF4\x8F\xBF\xBF"; /* 0xF4 0x8F 0xBF 0xBF => U+10FFFF */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xF4);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x8F);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xBF);
    TEST_VERIFY (mbsinit (&s));
  }

  return 0;
}

static int
test_big5_hkscs (void)
{
  xsetlocale (LC_ALL, "zh_HK.BIG5-HKSCS");

  /* A double byte character that maps to a pair of two byte UTF-8 code unit
     sequences.  */
  {
    const char *mbs = "\x88\x62"; /* 0x88 0x62 => U+00CA U+0304 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 2);
    mbs += 2;
    TEST_COMPARE (buf[0], 0xC3);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x8A);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xCC);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x84);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\x88\x62"; /* 0x88 0x62 => U+00CA U+0304 */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xC3);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x8A);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xCC);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x84);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Another double byte character that maps to a pair of two byte UTF-8 code
     unit sequences.  */
  {
    const char *mbs = "\x88\xA5"; /* 0x88 0xA5 => U+00EA U+030C */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) 2);
    mbs += 2;
    TEST_COMPARE (buf[0], 0xC3);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xAA);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xCC);
    TEST_COMPARE (mbrtoc8 (buf, mbs, strlen (mbs) + 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x8C);
    TEST_VERIFY (mbsinit (&s));
  }

  /* Same as last test, but one code unit at a time.  */
  {
    const char *mbs = "\x88\xA5"; /* 0x88 0xA5 => U+00EA U+030C */
    char8_t buf[1] = { 0 };
    mbstate_t s = { 0 };

    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -2);
    mbs += 1;
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) 1);
    mbs += 1;
    TEST_COMPARE (buf[0], 0xC3);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xAA);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0xCC);
    TEST_COMPARE (mbrtoc8 (buf, mbs, 1, &s), (size_t) -3);
    TEST_COMPARE (buf[0], 0x8C);
    TEST_VERIFY (mbsinit (&s));
  }

  return 0;
}

static int
do_test (void)
{
  test_utf8 ();
  test_big5_hkscs ();
  return 0;
}

#include <support/test-driver.c>
