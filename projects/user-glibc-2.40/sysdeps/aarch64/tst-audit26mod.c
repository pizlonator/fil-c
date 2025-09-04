/* Check LD_AUDIT for aarch64 ABI specifics.
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

#include <stdlib.h>
#include "tst-audit26mod.h"

struct large_struct
tst_audit26_func (char a, short b, long int c)
{
  if (a != ARG1)
    abort ();
  if (b != ARG2)
    abort ();
  if (c != ARG3)
    abort ();

  return set_large_struct (a, b, c);
}
