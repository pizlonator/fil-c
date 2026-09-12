/* Test exception in current environment.
   Copyright (C) 1997-2026 Free Software Foundation, Inc.
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

#include <fenv.h>
#include <unistd.h>
#include <ldsodefs.h>
#include <math-inline-asm.h>

int
__fetestexcept (int excepts)
{
  short temp;
  unsigned int xtemp = 0;

  /* Get current exceptions.  */
  __asm__ ("fnstsw %0" : "=a" (temp));

  /* If the CPU supports SSE we test the MXCSR as well.  */
  if (CPU_FEATURE_USABLE (SSE))
    stmxcsr_inline_asm (&xtemp);

  return (temp | xtemp) & excepts & FE_ALL_EXCEPT;
}
libm_hidden_def (__fetestexcept)
weak_alias (__fetestexcept, fetestexcept)
libm_hidden_def (fetestexcept)
