/* x86_64 soft-fp exception handling for _Float128.
   Copyright (C) 2026 Free Software Foundation, Inc.
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

#ifndef HAVE_SFP_HANDLE_EXCEPTIONS
#include <fenv.h>
#include <float.h>
#include <soft-fp.h>

#ifdef __SSE_MATH__
# define __math_force_eval_div(x, y) \
  do { asm ("" : "+x" (x)); asm volatile ("" : : "x" (x / y)); } while (0)
#else
# define __math_force_eval_div(x, y) \
  do { asm ("" : "+t" (x)); asm volatile ("" : : "f" (x / y)); } while (0)
#endif

void
__sfp_handle_exceptions (int _fex)
{
  fenv_t temp;

  if (_fex & FP_EX_INVALID)
    {
      float f = 0.0f;
      __math_force_eval_div (f, f);
    }
  if (_fex & FP_EX_DENORM)
    {
      asm volatile ("fnstenv\t%0" : "=m" (temp));
      temp.__status_word |= FP_EX_DENORM;
      asm volatile ("fldenv\t%0" : : "m" (temp));
      asm volatile ("fwait");
    }
  if (_fex & FP_EX_DIVZERO)
    {
      float f = 1.0f, g = 0.0f;
      __math_force_eval_div (f, g);
    }
  if (_fex & FP_EX_OVERFLOW)
    {
      asm volatile ("fnstenv\t%0" : "=m" (temp));
      temp.__status_word |= FP_EX_OVERFLOW;
      asm volatile ("fldenv\t%0" : : "m" (temp));
      asm volatile ("fwait");
    }
  if (_fex & FP_EX_UNDERFLOW)
    {
      asm volatile ("fnstenv\t%0" : "=m" (temp));
      temp.__status_word |= FP_EX_UNDERFLOW;
      asm volatile ("fldenv\t%0" : : "m" (temp));
      asm volatile ("fwait");
    }
  if (_fex & FP_EX_INEXACT)
    {
      float f = 1.0f, g = 3.0f;
      __math_force_eval_div (f, g);
    }
}
#endif
