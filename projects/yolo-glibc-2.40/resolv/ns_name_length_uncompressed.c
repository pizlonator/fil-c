/* Skip over an uncompressed name in wire format.
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
#include <errno.h>
#include <stdbool.h>

int
__ns_name_length_uncompressed (const unsigned char *p,
                                const unsigned char *eom)
{
  const unsigned char *start = p;

  while (true)
    {
      if (p == eom)
        {
          /* Truncated packet: no room for label length.  */
          __set_errno (EMSGSIZE);
          return -1;
        }

      unsigned char b = *p;
      ++p;
      if (b == 0)
        {
          /* Root label.  */
          size_t length = p - start;
          if (length > NS_MAXCDNAME)
            {
              /* Domain name too long.  */
              __set_errno (EMSGSIZE);
              return -1;
            }
          return length;
        }

      if (b <= 63)
        {
          /* Regular label.  */
          if (b <= eom - p)
            p += b;
          else
            {
              /* Truncated packet: label incomplete.  */
              __set_errno (EMSGSIZE);
              return -1;
            }
        }
      else
        {
          /* Compression reference or corrupted label length.  */
          __set_errno (EMSGSIZE);
          return -1;
        }
    }
}
