/* Copyright (C) 1997-2026 Free Software Foundation, Inc.

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <fenv.h>
#include <fpu_control.h>
#include <float.h>
#include <stdint.h>

int
__feraiseexcept (int excepts)
{
  /* Fil-C: the upstream AArch64 version of this file raises each exception
     with hand-written inline assembly (ldr/fdiv/fadd followed by mrs fpsr).
     The Fil-C compiler's safe inline asm rejects the "m" (memory) constraints
     and the ldr instructions used here, so raise the exceptions with plain C
     arithmetic on volatile variables (the same trick the upstream code
     conceptually relies on; the compiler cannot constant-fold through the
     volatile reads) and then read the FPSR through _FPU_GETFPSR so that the
     exception flags are visible immediately, like the asm did.

     Note that raising FE_OVERFLOW also raises FE_INEXACT, which is
     inherent to how the exception is produced and matches upstream.  */
  volatile float fp_zero = 0.0f;
  volatile float fp_one = 1.0f;
  volatile float fp_max = FLT_MAX;
  volatile float fp_min = FLT_MIN;
  volatile float fp_1e32 = 1.0e32f;
  volatile float fp_two = 2.0f;
  volatile float fp_three = 3.0f;
  uint64_t fpsr;

  if (FE_INVALID & excepts)
    {
      float r = fp_zero / fp_zero;
      (void) r;
      _FPU_GETFPSR (fpsr);
    }

  if (FE_DIVBYZERO & excepts)
    {
      float r = fp_one / fp_zero;
      (void) r;
      _FPU_GETFPSR (fpsr);
    }

  if (FE_OVERFLOW & excepts)
    {
      /* There's no way to raise overflow without also raising inexact.  */
      float r = fp_max + fp_1e32;
      (void) r;
      _FPU_GETFPSR (fpsr);
    }

  if (FE_UNDERFLOW & excepts)
    {
      float r = fp_min / fp_three;
      (void) r;
      _FPU_GETFPSR (fpsr);
    }

  if (FE_INEXACT & excepts)
    {
      float r = fp_two / fp_three;
      (void) r;
      _FPU_GETFPSR (fpsr);
    }

  return 0;
}
libm_hidden_def (__feraiseexcept)
static_weak_alias (__feraiseexcept, feraiseexcept)
libm_hidden_weak (feraiseexcept)
