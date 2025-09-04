/* fminimum_num().  LoongArch version.
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

#define NO_MATH_REDIRECT
#include <math.h>
#include <libm-alias-double.h>
#include <fpu_control.h>

double
__fminimum_num (double x, double y)
{
  int x_cond;
  int y_cond;
  asm volatile ("fclass.d \t%0, %1" : "=f" (x_cond) : "f" (x));
  asm volatile ("fclass.d \t%0, %1" : "=f" (y_cond) : "f" (y));

  if (__glibc_unlikely((x_cond & _FCLASS_NAN) && !(y_cond & _FCLASS_NAN)))
    {
      asm volatile ("fmin.d \t%0, %1, %2" : "=f" (x) : "f" (x), "f" (y));
      return y;
    }
  else if (__glibc_unlikely(!(x_cond & _FCLASS_NAN) && (y_cond & _FCLASS_NAN)))
    {
      asm volatile ("fmin.d \t%0, %1, %2" : "=f" (y) : "f" (x), "f" (y));
      return x;
    }
  else
    {
      asm volatile ("fmin.d \t%0, %1, %2" : "=f" (x) : "f" (x), "f" (y));
      return x;
    }
}
libm_alias_double (__fminimum_num, fminimum_num)
