/* Install given floating-point environment.
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
#include <pizlonated_math.h>

/* The upstream x86_64 __fesetenv reads and writes the FPU environment with fnstenv/fldenv/stmxcsr/
   ldmxcsr inline asm whose operands are pointers, which Fil-C cannot instrument. Run the whole
   operation -- including the FE_DFL_ENV / FE_NOMASK_ENV handling -- out of line in the Yolo runtime
   (see zmath_fesetenv), which forwards to the underlying libc's __fesetenv.  */
int
__fesetenv (const fenv_t *envp)
{
  return zmath_fesetenv (envp);
}
libm_hidden_def (__fesetenv)
static_weak_alias (__fesetenv, fesetenv)
libm_hidden_weak (fesetenv)
