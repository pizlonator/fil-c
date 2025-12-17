/* Conversion UCS-4 to UTF-16.
   Copyright (C) 2002, 2006-2007, 2009-2024 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2002.

   This file is free software.
   It is dual-licensed under "the GNU LGPLv3+ or the GNU GPLv2+".
   You can redistribute it and/or modify it under either
     - the terms of the GNU Lesser General Public License as published
       by the Free Software Foundation, either version 3, or (at your
       option) any later version, or
     - the terms of the GNU General Public License as published by the
       Free Software Foundation; either version 2, or (at your option)
       any later version, or
     - the same dual license "the GNU LGPLv3+ or the GNU GPLv2+".

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License and the GNU General Public License
   for more details.

   You should have received a copy of the GNU Lesser General Public
   License and of the GNU General Public License along with this
   program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include "unistr.h"

int
u16_uctomb_aux (uint16_t *s, ucs4_t uc, ptrdiff_t n)
{
  if (uc < 0xd800)
    {
      /* The case n >= 1 is already handled by the caller.  */
    }
  else if (uc < 0x10000)
    {
      if (uc >= 0xe000)
        {
          if (n >= 1)
            {
              s[0] = uc;
              return 1;
            }
        }
      else
        return -1;
    }
  else
    {
      if (uc < 0x110000)
        {
          if (n >= 2)
            {
              s[0] = 0xd800 + ((uc - 0x10000) >> 10);
              s[1] = 0xdc00 + ((uc - 0x10000) & 0x3ff);
              return 2;
            }
        }
      else
        return -1;
    }
  return -2;
}
