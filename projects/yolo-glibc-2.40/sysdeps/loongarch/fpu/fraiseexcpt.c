/* Raise given exceptions.
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

#include <fenv.h>
#include <fpu_control.h>
#include <float.h>

int
__feraiseexcept (int excepts)
{
  const float fp_zero = 0.0f;
  const float fp_one = 1.0f;
  const float fp_max = FLT_MAX;
  const float fp_min = FLT_MIN;
  const float fp_1e32 = 1.0e32f;
  const float fp_two = 2.0f;
  const float fp_three = 3.0f;

  /* Raise exceptions represented by EXPECTS.  But we must raise only
     one signal at a time.  It is important that if the overflow/underflow
     exception and the inexact exception are given at the same time,
     the overflow/underflow exception follows the inexact exception.  */

  /* First: invalid exception.  */
  if (FE_INVALID & excepts)
    __asm__ __volatile__("fdiv.s $f0,%0,%0\n\t"
			 :
			 : "f"(fp_zero)
			 : "$f0");

  /* Next: division by zero.  */
  if (FE_DIVBYZERO & excepts)
    __asm__ __volatile__("fdiv.s $f0,%0,%1\n\t"
			 :
			 : "f"(fp_one), "f"(fp_zero)
			 : "$f0");

  /* Next: overflow.  */
  if (FE_OVERFLOW & excepts)
    /* There's no way to raise overflow without also raising inexact.  */
    __asm__ __volatile__("fadd.s $f0,%0,%1\n\t"
			 :
			 : "f"(fp_max), "f"(fp_1e32)
			 : "$f0");

  /* Next: underflow.  */
  if (FE_UNDERFLOW & excepts)
    __asm__ __volatile__("fdiv.s $f0,%0,%1\n\t"
			 :
			 : "f"(fp_min), "f"(fp_three)
			 : "$f0");

  /* Last: inexact.  */
  if (FE_INEXACT & excepts)
    __asm__ __volatile__("fdiv.s $f0, %0, %1\n\t"
			 :
			 : "f"(fp_two), "f"(fp_three)
			 : "$f0");

  /* Success.  */
  return 0;
}

libm_hidden_def (__feraiseexcept) weak_alias (__feraiseexcept, feraiseexcept)
libm_hidden_weak (feraiseexcept)
