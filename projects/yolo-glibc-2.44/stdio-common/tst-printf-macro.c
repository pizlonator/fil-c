/* Test printf PRI* macro narrowing arguments.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
#include <stdio.h>
#include <string.h>

#include <libc-diag.h>
#include <support/check.h>

#define CHECK_PRINTF(EXPECTED, FMT, ...)		\
  do							\
    {							\
      int ret = snprintf (buf, sizeof buf, FMT,		\
			  __VA_ARGS__);			\
      TEST_COMPARE_STRING (buf, EXPECTED);		\
      TEST_COMPARE (ret, strlen (EXPECTED));		\
    }							\
  while (0)

_Static_assert (INT_FAST8_WIDTH == 8, "width of int_fast8_t");
_Static_assert (UINT_FAST8_WIDTH == 8, "width of uint_fast8_t");

static int
do_test (void)
{
  char buf[1024];
  CHECK_PRINTF ("-121", "%" PRId8, 1234567);
  CHECK_PRINTF ("-121", "%" PRIdLEAST8, 1234567);
  CHECK_PRINTF ("-121", "%" PRIdFAST8, 1234567);
  CHECK_PRINTF ("-10617", "%" PRId16, 1234567);
  CHECK_PRINTF ("-10617", "%" PRIdLEAST16, 1234567);
  CHECK_PRINTF ("-121", "%" PRIi8, 1234567);
  CHECK_PRINTF ("-121", "%" PRIiLEAST8, 1234567);
  CHECK_PRINTF ("-121", "%" PRIiFAST8, 1234567);
  CHECK_PRINTF ("-10617", "%" PRIi16, 1234567);
  CHECK_PRINTF ("-10617", "%" PRIiLEAST16, 1234567);
  CHECK_PRINTF ("207", "%" PRIo8, 1234567);
  CHECK_PRINTF ("207", "%" PRIoLEAST8, 1234567);
  CHECK_PRINTF ("207", "%" PRIoFAST8, 1234567);
  CHECK_PRINTF ("153207", "%" PRIo16, 1234567);
  CHECK_PRINTF ("153207", "%" PRIoLEAST16, 1234567);
  CHECK_PRINTF ("135", "%" PRIu8, 1234567);
  CHECK_PRINTF ("135", "%" PRIuLEAST8, 1234567);
  CHECK_PRINTF ("135", "%" PRIuFAST8, 1234567);
  CHECK_PRINTF ("54919", "%" PRIu16, 1234567);
  CHECK_PRINTF ("54919", "%" PRIuLEAST16, 1234567);
  CHECK_PRINTF ("87", "%" PRIx8, 1234567);
  CHECK_PRINTF ("87", "%" PRIxLEAST8, 1234567);
  CHECK_PRINTF ("87", "%" PRIxFAST8, 1234567);
  CHECK_PRINTF ("d687", "%" PRIx16, 1234567);
  CHECK_PRINTF ("d687", "%" PRIxLEAST16, 1234567);
  CHECK_PRINTF ("87", "%" PRIX8, 1234567);
  CHECK_PRINTF ("87", "%" PRIXLEAST8, 1234567);
  CHECK_PRINTF ("87", "%" PRIXFAST8, 1234567);
  CHECK_PRINTF ("D687", "%" PRIX16, 1234567);
  CHECK_PRINTF ("D687", "%" PRIXLEAST16, 1234567);
  /* GCC does not know the %b or %B formats before GCC 12.  */
  DIAG_PUSH_NEEDS_COMMENT;
#if !__GNUC_PREREQ (12, 0)
  DIAG_IGNORE_NEEDS_COMMENT (11, "-Wformat");
  DIAG_IGNORE_NEEDS_COMMENT (11, "-Wformat-extra-args");
#endif
  CHECK_PRINTF ("10000111", "%" PRIb8, 1234567);
  CHECK_PRINTF ("10000111", "%" PRIbLEAST8, 1234567);
  CHECK_PRINTF ("10000111", "%" PRIbFAST8, 1234567);
  CHECK_PRINTF ("1101011010000111", "%" PRIb16, 1234567);
  CHECK_PRINTF ("1101011010000111", "%" PRIbLEAST16, 1234567);
  CHECK_PRINTF ("10000111", "%" PRIB8, 1234567);
  CHECK_PRINTF ("10000111", "%" PRIBLEAST8, 1234567);
  CHECK_PRINTF ("10000111", "%" PRIBFAST8, 1234567);
  CHECK_PRINTF ("1101011010000111", "%" PRIB16, 1234567);
  CHECK_PRINTF ("1101011010000111", "%" PRIBLEAST16, 1234567);
  DIAG_POP_NEEDS_COMMENT;
  return 0;
}

#include <support/test-driver.c>
