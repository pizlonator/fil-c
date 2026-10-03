/* Disable floating-point exceptions.
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
#include <fpu_control.h>

int
fedisableexcept (int excepts)
{
  fpu_control_t new_exc;
  unsigned short int old_exc;
  unsigned int new;

  excepts &= FE_ALL_EXCEPT;

  /* Get the current control word of the x87 FPU.  */
  _FPU_GETCW (new_exc);

  old_exc = (~new_exc) & FE_ALL_EXCEPT;

  new_exc |= excepts;
  _FPU_SETCW (new_exc);

  /* And now the same for the SSE MXCSR register.  */
  stmxcsr_inline_asm (&new);

  /* The SSE exception masks are shifted by 7 bits.  */
  new |= excepts << 7;
  ldmxcsr_inline_asm (&new);

  return old_exc;
}
