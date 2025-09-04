/* Tests for Unicode character output.

   Copyright (C) 2020-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible, 2020.  */

#include <config.h>

/* Specification.  */
#include "unicodeio.h"

#include <locale.h>
#include <string.h>

#include "localcharset.h"
#include "macros.h"

#define TEST_CODE 0x2022
#define TEST_CODE_AS_UTF8 "\xe2\x80\xa2"
#define TEST_CODE_AS_GB18030 "\x81\x36\xa6\x31"

static char result[64];

static long
success_callback (const char *buf, size_t buflen, void *callback_arg)
{
  memcpy (result, buf, buflen);
  result[buflen] = '\0';
  return 42;
}

static long
failure_callback (unsigned int code, const char *msg, void *callback_arg)
{
  ASSERT (code == TEST_CODE);
  strcpy (result, ".");
  return 55;
}

int
main (int argc, char *argv[])
{
  /* configure should already have checked that the locale is supported.  */
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  switch (unicode_to_mb (TEST_CODE, success_callback, failure_callback, NULL))
    {
    case 42:
      if (argc > 1)
        switch (argv[1][0])
          {
          case '1':
            /* On some platforms, the "C" locale has UTF-8 encoding.
               And on native Windows, the "C" locale may have an 8-bit encoding
               such as CP1252, that contains the U+2022 character.  */
            {
              const char *charset = locale_charset ();
              if (strcmp (charset, "CP874") == 0
                  || strcmp (charset, "CP1250") == 0
                  || strcmp (charset, "CP1251") == 0
                  || strcmp (charset, "CP1252") == 0
                  || strcmp (charset, "CP1253") == 0
                  || strcmp (charset, "CP1254") == 0
                  || strcmp (charset, "CP1255") == 0
                  || strcmp (charset, "CP1256") == 0
                  || strcmp (charset, "CP1257") == 0
                  || strcmp (charset, "CP1258") == 0)
                ASSERT (strcmp (result, "\x95") == 0);
              else
                ASSERT (strcmp (result, TEST_CODE_AS_UTF8) == 0);
            }
            break;
          case '2':
            ASSERT (strcmp (result, TEST_CODE_AS_UTF8) == 0);
            break;
          case '3':
            ASSERT (strcmp (result, TEST_CODE_AS_GB18030) == 0);
            break;
          }
      break;
    case 55:
      ASSERT (strcmp (result, ".") == 0);
      break;
    default:
      ASSERT (0);
    }

  return 0;
}
