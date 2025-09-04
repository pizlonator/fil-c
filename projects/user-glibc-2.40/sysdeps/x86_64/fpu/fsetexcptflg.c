/* Set floating-point environment exception handling.
   Copyright (C) 2001-2024 Free Software Foundation, Inc.
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
#include <math.h>

int
fesetexceptflag (const fexcept_t *flagp, int excepts)
{
  /* The flags can be set in the 387 unit or in the SSE unit.
     When we need to clear a flag, we need to do so in both units,
     due to the way fetestexcept() is implemented.
     When we need to set a flag, it is sufficient to do it in the SSE unit,
     because that is guaranteed to not trap.  */

  fenv_t temp;
  unsigned int mxcsr;

  excepts &= FE_ALL_EXCEPT;

  /* Get the current x87 FPU environment.  We have to do this since we
     cannot separately set the status word.  */
  __asm__ ("fnstenv %0" : "=m" (*&temp));

  /* Clear relevant flags.  */
  temp.__status_word &= ~(excepts & ~ *flagp);

  /* Store the new status word (along with the rest of the environment).  */
  __asm__ ("fldenv %0" : : "m" (*&temp));

  /* And now similarly for SSE.  */
  __asm__ ("stmxcsr %0" : "=m" (*&mxcsr));

  /* Clear or set relevant flags.  */
  mxcsr ^= (mxcsr ^ *flagp) & excepts;

  /* Put the new data in effect.  */
  __asm__ ("ldmxcsr %0" : : "m" (*&mxcsr));

  /* Success.  */
  return 0;
}
