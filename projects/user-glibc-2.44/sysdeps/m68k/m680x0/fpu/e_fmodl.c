/* Fmod function, m68k double version.
   Copyright (C) 1996-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <math.h>
#include <libm-alias-finite.h>
#include "mathimpl.h"

long double
__ieee754_fmodl (long double x, long double y)
{
  return __m81_u(__ieee754_fmodl)(x, y);
}
libm_alias_finite (__ieee754_fmodl, __fmodl)
