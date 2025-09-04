/* Compare two binary domain names for quality.
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

#include <arpa/nameser.h>
#include <stdbool.h>

/* Convert ASCII letters to upper case.  */
static inline int
ascii_toupper (unsigned char ch)
{
  if (ch >= 'a' && ch <= 'z')
    return ch - 'a' + 'A';
  else
    return ch;
}

bool
__ns_samebinaryname (const unsigned char *a, const unsigned char *b)
{
  while (*a != 0 && *b != 0)
    {
      if (*a != *b)
        /* Different label length.  */
        return false;
      int labellen = *a;
      ++a;
      ++b;
      for (int i = 0; i < labellen; ++i)
        {
          if (*a != *b && ascii_toupper (*a) != ascii_toupper (*b))
            /* Different character in label.  */
            return false;
          ++a;
          ++b;
        }
    }

  /* Match if both names are at the root label.  */
  return *a == 0 && *b == 0;
}
