/* Test that an array of strings does not contain duplicates.

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

#include <string.h>
#include <stdlib.h>
#include <support/check.h>

static int
compare_strings (const void *a, const void *b)
{
  const char *stra = * (const char **) a;
  const char *strb = * (const char **) b;

  int ret = strcmp (stra, strb);

  if (!ret)
    FAIL_EXIT1 ("Found duplicate strings: \"%s\"\n", stra);

  return ret;
}

/* We check for and fail on duplicate strings in the comparator.  */
#define VERIFY_UNIQUE_STRINGS(strarray, narray) \
  qsort ((strarray), (narray), sizeof (char *), compare_strings)
