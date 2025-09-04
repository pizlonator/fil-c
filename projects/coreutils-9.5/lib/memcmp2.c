/* Compare two memory areas with possibly different lengths.
   Copyright (C) 2009-2024 Free Software Foundation, Inc.

   This file is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include "memcmp2.h"

#include <string.h>

int
memcmp2 (const char *s1, size_t n1, const char *s2, size_t n2)
{
  int cmp = memcmp (s1, s2, n1 <= n2 ? n1 : n2);
  if (cmp == 0)
    cmp = _GL_CMP (n1, n2);
  return cmp;
}
