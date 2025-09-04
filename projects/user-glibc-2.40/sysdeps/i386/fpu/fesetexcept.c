/* Set given exception flags.  i386 version.
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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
#include <ldsodefs.h>

int
fesetexcept (int excepts)
{
  /* The flags can be set in the 387 unit or in the SSE unit.  To set a flag,
     it is sufficient to do it in the SSE unit, because that is guaranteed to
     not trap.  However, on i386 CPUs that have only a 387 unit, set the flags
     in the 387, as long as this cannot trap.  */

  excepts &= FE_ALL_EXCEPT;

  if (CPU_FEATURE_USABLE (SSE))
    {
      /* Get the control word of the SSE unit.  */
      unsigned int mxcsr;
      __asm__ ("stmxcsr %0" : "=m" (*&mxcsr));

      /* Set relevant flags.  */
      mxcsr |= excepts;

      /* Put the new data in effect.  */
      __asm__ ("ldmxcsr %0" : : "m" (*&mxcsr));
    }
  else
    {
      fenv_t temp;

      /* Note: fnstenv masks all floating-point exceptions until the fldenv
	 or fldcw below.  */
      __asm__ ("fnstenv %0" : "=m" (*&temp));

      /* Set relevant flags.  */
      temp.__status_word |= excepts;

      if ((~temp.__control_word) & excepts)
	{
	  /* Setting the exception flags may trigger a trap (at the next
	     floating-point instruction, but that does not matter).
	     ISO C23 (7.6.4.4) does not allow it.  */
	  __asm__ volatile ("fldcw %0" : : "m" (*&temp.__control_word));
	  return -1;
	}

      /* Store the new status word (along with the rest of the environment).  */
      __asm__ ("fldenv %0" : : "m" (*&temp));
    }

  return 0;
}
