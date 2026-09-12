/* DSO used by tst-bz34144.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

void
test_float_args (double a, double b, double c, double d,
		 double e, double f, double g, double h)
{
  if (a != 2.0 || b != 3.0 || c != 4.0 || d != 5.0
      || e != 6.0 || f != 7.0 || g != 8.0 || h != 9.0)
    abort ();
}
