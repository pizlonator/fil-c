/* scalbf().  LoongArch version.
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
#include <libm-alias-finite.h>
#include <fpu_control.h>
#include <float.h>

float
__ieee754_scalbf (float x, float fn)
{
  int x_cond;
  int fn_cond;
  asm volatile ("fclass.s \t%0, %1" : "=f" (x_cond) : "f" (x));
  asm volatile ("fclass.s \t%0, %1" : "=f" (fn_cond) : "f" (fn));

  if (__glibc_unlikely(( x_cond | fn_cond) & _FCLASS_NAN))
      return x * fn;
  else if (__glibc_unlikely(fn_cond & _FCLASS_INF))
    {
      if (!(fn_cond & _FCLASS_MINF))
	  return x * fn;
      else
	  return x / -fn;
    }
  else if (__glibc_likely(-FLT_MAX < fn && fn < FLT_MAX))
    {
      float rintf_fn, tmp;

      /* rintf_fn = rintf(fn) */
      asm volatile ("frint.s \t%0, %1" : "=f" (rintf_fn) : "f" (fn));

      if (rintf_fn != fn )
	  return (x - x) / (x - x);

      asm volatile ("ftintrz.w.s \t%0, %1" : "=f" (tmp) : "f" (rintf_fn));
      asm volatile ("fscaleb.s \t%0, %1, %2" : "=f" (x) : "f" (x), "f" (tmp));
    }
  else
    asm volatile ("fscaleb.s \t%0, %1, %2" : "=f" (x) : "f" (x), "f" (fn));

  return x;
}
libm_alias_finite (__ieee754_scalbf, __scalbf)
