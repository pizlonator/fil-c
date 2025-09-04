/* Test of strtold() in a French locale.
   Copyright (C) 2019-2024 Free Software Foundation, Inc.

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

#include <config.h>

/* Specification.  */
#include <stdlib.h>

#include <errno.h>
#include <locale.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  /* Try to set the locale by implicitly looking at the LC_ALL environment
     variable.
     configure should already have checked that the locale is supported.  */
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

  {
    const char input[] = "1,";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    ASSERT (result == 1.0L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = ",5";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    ASSERT (result == 0.5L);
    ASSERT (ptr == input + 2);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1,5";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    ASSERT (result == 1.5L);
    ASSERT (ptr == input + 3);
    ASSERT (errno == 0);
  }
  {
    const char input[] = "1.5";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    /* On AIX 7.2, in the French locale, '.' is recognized as an alternate
       radix character.  */
    ASSERT ((ptr == input + 1 && result == 1.0L)
            || (ptr == input + 3 && result == 1.5L));
    ASSERT (errno == 0);
  }
  {
    const char input[] = "123.456,789";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    /* On AIX 7.2, in the French locale, '.' is recognized as an alternate
       radix character.  */
    ASSERT ((ptr == input + 3 && result == 123.0L)
            || (ptr == input + 7 && result > 123.45L && result < 123.46L));
    ASSERT (errno == 0);
  }
  {
    const char input[] = "123,456.789";
    char *ptr;
    long double result;
    errno = 0;
    result = strtold (input, &ptr);
    ASSERT (result > 123.45L && result < 123.46L);
    ASSERT (ptr == input + 7);
    ASSERT (errno == 0);
  }

  return 0;
}
