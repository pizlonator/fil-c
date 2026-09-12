/* Clear given exceptions in current floating-point environment.
   Copyright (C) 2001-2026 Free Software Foundation, Inc.
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
#include <math-inline-asm.h>

int
__feclearexcept (int excepts)
{
  fenv_t temp;
  unsigned int mxcsr;

  /* Mask out unsupported bits/exceptions.  */
  excepts &= FE_ALL_EXCEPT;

  /* Bah, we have to clear selected exceptions.  Since there is no
     `fldsw' instruction we have to do it the hard way.  */
  __asm__ ("fnstenv %0" : "=m" (temp));

  /* Clear the relevant bits.  */
  temp.__status_word &= excepts ^ FE_ALL_EXCEPT;

  /* Put the new data in effect.  */
  __asm__ ("fldenv %0" : : "m" (temp));

  /* And the same procedure for SSE.  */
  stmxcsr_inline_asm (&mxcsr);

  /* Clear the relevant bits.  */
  mxcsr &= ~excepts;

  /* And put them into effect.  */
  ldmxcsr_inline_asm (&mxcsr);

  /* Success.  */
  return 0;
}
libm_hidden_def (__feclearexcept)
static_weak_alias (__feclearexcept, feclearexcept)
libm_hidden_def (feclearexcept)
