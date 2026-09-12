/* Raise given exceptions.  OpenRISC version.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#include <fenv.h>
#include <fpu_control.h>
#include <float.h>
#include <math.h>

int
__feraiseexcept (int excepts)
{
  if (excepts == 0)
    return 0;

  /* Raise exceptions represented by EXPECTS.  */

  if (excepts & FE_INEXACT)
  {
    float d = 1.0, x = 3.0;
    __asm__ volatile ("lf.div.s %0, %0, %1" : "+r" (d) : "r" (x));
  }

  if (excepts & FE_UNDERFLOW)
  {
    float d = FLT_MIN;
    __asm__ volatile ("lf.mul.s %0, %0, %0" : "+r" (d));
  }

  if (excepts & FE_OVERFLOW)
  {
    float d = FLT_MAX;
    __asm__ volatile ("lf.mul.s %0, %0, %0" : "+r" (d) : "r" (d));
  }

  if (excepts & FE_DIVBYZERO)
  {
    float d = 1.0, x = 0.0;
    __asm__ volatile ("lf.div.s %0, %0, %1" : "+r" (d) : "r" (x));
  }

  if (excepts & FE_INVALID)
  {
    float d = HUGE_VAL, x = 0.0;
    __asm__ volatile ("lf.mul.s %0, %1, %0" : "+r" (d) : "r" (x));
  }

  /* Success.  */
  return 0;
}
libm_hidden_def (__feraiseexcept)
weak_alias (__feraiseexcept, feraiseexcept)
libm_hidden_weak (feraiseexcept)
