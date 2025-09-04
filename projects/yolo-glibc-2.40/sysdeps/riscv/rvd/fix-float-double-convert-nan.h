/* Fix for conversion of float NAN to double.  RISC-V version..
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef FIX_FLOAT_DOUBLE_CONVERT_NAN_H
#define FIX_FLOAT_DOUBLE_CONVERT_NAN_H

#include <math.h>

/* RISC-V rvd instructions do not preserve the signbit of NAN
   when converting from float to double.  */
static inline double
keep_sign_conversion (float flt)
{
  if (__glibc_unlikely (isnan (flt)))
  {
    float x = copysignf (1.f, flt);
    return copysign ((double) flt, (double) x);
  }
  return flt;
}

#endif
