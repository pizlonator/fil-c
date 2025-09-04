/* Test case for strncmp inside a transactionally executing RTM region.
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

#include <stdint.h>
#include <tst-string-rtm.h>

#ifdef WIDE
# define CHAR wchar_t
# define MEMSET wmemset
# define STRNCMP wcsncmp
# define TEST_NAME "wcsncmp"
#else /* !WIDE */
# define CHAR char
# define MEMSET memset
# ifndef STRNCMP
#  define STRNCMP strncmp
#  define TEST_NAME "strncmp"
# endif
#endif /* !WIDE */



#define LOOP 3000
#define STRING_SIZE 1024
CHAR string1[STRING_SIZE];
CHAR string2[STRING_SIZE];

__attribute__ ((noinline, noclone))
static int
prepare (void)
{
  MEMSET (string1, 'a', STRING_SIZE - 1);
  MEMSET (string2, 'a', STRING_SIZE - 1);
  if (STRNCMP (string1, string2, STRING_SIZE) == 0)
    return EXIT_SUCCESS;
  else
    return EXIT_FAILURE;
}

__attribute__ ((noinline, noclone))
static int
function (void)
{
  if (STRNCMP (string1, string2, STRING_SIZE) == 0)
    return 0;
  else
    return 1;
}

__attribute__ ((noinline, noclone))
static int
function_overflow (void)
{
  if (STRNCMP (string1, string2, SIZE_MAX) == 0)
    return 0;
  else
    return 1;
}

__attribute__ ((noinline, noclone))
static int
function_overflow2 (void)
{
  if (STRNCMP (string1, string2, SIZE_MAX >> 4) == 0)
    return 0;
  else
    return 1;
}

static int
do_test (void)
{
  int status = do_test_1 (TEST_NAME, LOOP, prepare, function);
  if (status != EXIT_SUCCESS)
    return status;
  status = do_test_1 (TEST_NAME, LOOP, prepare, function_overflow);
  if (status != EXIT_SUCCESS)
    return status;
  status = do_test_1 (TEST_NAME, LOOP, prepare, function_overflow2);
  if (status != EXIT_SUCCESS)
    return status;
  return status;
}
