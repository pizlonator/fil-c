/* Fix for conversion of float NAN to double.  Generic version.
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

/* This function aims to work around conversions of float -NAN
   to double returning NAN instead of the correct -NAN in some
   architectures.  */
static inline double __attribute__ ((always_inline))
keep_sign_conversion (float flt)
{
  return flt;
}

#endif
