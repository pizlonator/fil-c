/* Store current floating-point environment and clear exceptions.
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
__feholdexcept (fenv_t *envp)
{
  unsigned int mxcsr;

  /* Store the environment.  Recall that fnstenv has a side effect of
     masking all exceptions.  Then clear all exceptions.  */
  asm volatile ("fnstenv %0" : "=m" (*envp));
  stmxcsr_inline_asm (&envp->__mxcsr);
  asm volatile ("fnclex" : "=m" (*envp));

  /* Set the SSE MXCSR register.  */
  mxcsr = (envp->__mxcsr | 0x1f80) & ~0x3f;
  ldmxcsr_inline_asm (&mxcsr);

  return 0;
}
libm_hidden_def (__feholdexcept)
static_weak_alias (__feholdexcept, feholdexcept)
libm_hidden_weak (feholdexcept)
