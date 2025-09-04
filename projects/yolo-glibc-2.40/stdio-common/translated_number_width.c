/* Compute the printf width of a sequence of ASCII digits.
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

#include <assert.h>
#include <limits.h>
#include <locale/localeinfo.h>
#include <printf.h>

int
__translated_number_width (locale_t loc, const char *first, const char *last)
{
  struct lc_ctype_data *ctype = loc->__locales[LC_CTYPE]->private;

  if (ctype->outdigit_bytes_all_equal > 0)
    return (last - first) * ctype->outdigit_bytes_all_equal;
  else
    {
      /* Digits have varying length, so the fast path cannot be used.  */
      int digits = 0;
      for (const char *p = first; p < last; ++p)
        {
          assert ('0' <= *p && *p <= '9');
          digits += ctype->outdigit_bytes[*p - '0'];
        }
      return digits;
    }
}
