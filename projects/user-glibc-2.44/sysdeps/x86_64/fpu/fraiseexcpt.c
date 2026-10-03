/* Raise given exceptions.
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
#include <math.h>
#include <filc-x87-env.h>

int
__feraiseexcept (int excepts)
{
  /* Raise exceptions represented by EXPECTS.  But we must raise only
     one signal at a time.  It is important that if the overflow/underflow
     exception and the inexact exception are given at the same time,
     the overflow/underflow exception follows the inexact exception.  */

  /* First: invalid exception.  */
  if ((FE_INVALID & excepts) != 0)
    /* One example of an invalid operation is 0.0 / 0.0.  */
    divss_inline_asm (0.0f, 0.0f);

  /* Next: division by zero.  */
  if ((FE_DIVBYZERO & excepts) != 0)
    divss_inline_asm (1.0f, 0.0f);

  /* Next: overflow.  */
  if ((FE_OVERFLOW & excepts) != 0)
    filc_raise_flag (FE_OVERFLOW);

  /* Next: underflow.  */
  if ((FE_UNDERFLOW & excepts) != 0)
    filc_raise_flag (FE_UNDERFLOW);

  /* Last: inexact.  */
  if ((FE_INEXACT & excepts) != 0)
    filc_raise_flag (FE_INEXACT);

  /* Success.  */
  return 0;
}
libm_hidden_def (__feraiseexcept)
static_weak_alias (__feraiseexcept, feraiseexcept)
libm_hidden_weak (feraiseexcept)
