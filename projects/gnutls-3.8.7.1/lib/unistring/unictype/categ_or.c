/* Categories of Unicode characters.
   Copyright (C) 2007, 2009-2024 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2007.

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
#include "unictype.h"

uc_general_category_t
uc_general_category_or (uc_general_category_t category1,
                        uc_general_category_t category2)
{
  uint32_t bitmask;
  uc_general_category_t result;

  bitmask = category1.bitmask | category2.bitmask;

  if (bitmask == category1.bitmask)
    return category1;

  if (bitmask == category2.bitmask)
    return category2;

  result.bitmask = bitmask;
  result.generic = 1;
  result.lookup.lookup_fn = &uc_is_general_category_withtable;
  return result;
}
