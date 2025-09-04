/* Set floating-point environment exception handling.
   Copyright (C) 1997-2024 Free Software Foundation, Inc.
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
__fesetexceptflag (const fexcept_t *flagp, int excepts)
{
  /* The flags can be set in the 387 unit or in the SSE unit.  When we need to
     clear a flag, we need to do so in both units, due to the way fetestexcept
     is implemented.
     When we need to set a flag, it is sufficient to do it in the SSE unit,
     because that is guaranteed to not trap.  However, on i386 CPUs that have
     only a 387 unit, set the flags in the 387, as long as this cannot trap.  */

  fenv_t temp;

  excepts &= FE_ALL_EXCEPT;

  /* Get the current x87 FPU environment.  We have to do this since we
     cannot separately set the status word.
     Note: fnstenv masks all floating-point exceptions until the fldenv
     or fldcw below.  */
  __asm__ ("fnstenv %0" : "=m" (*&temp));

  if (CPU_FEATURE_USABLE (SSE))
    {
      unsigned int mxcsr;

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
    }
  else
    {
      /* Clear or set relevant flags.  */
      temp.__status_word ^= (temp.__status_word ^ *flagp) & excepts;

      if ((~temp.__control_word) & temp.__status_word & excepts)
	{
	  /* Setting the exception flags may trigger a trap (at the next
	     floating-point instruction, but that does not matter).
	     ISO C 23 § 7.6.4.5 does not allow it.  */
	  __asm__ volatile ("fldcw %0" : : "m" (*&temp.__control_word));
	  return -1;
	}

      /* Store the new status word (along with the rest of the environment).  */
      __asm__ ("fldenv %0" : : "m" (*&temp));
    }

  /* Success.  */
  return 0;
}

#include <shlib-compat.h>
#if SHLIB_COMPAT (libm, GLIBC_2_1, GLIBC_2_2)
strong_alias (__fesetexceptflag, __old_fesetexceptflag)
compat_symbol (libm, __old_fesetexceptflag, fesetexceptflag, GLIBC_2_1);
#endif

versioned_symbol (libm, __fesetexceptflag, fesetexceptflag, GLIBC_2_2);
