/* Nearest integet template for x86.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

#define NO_MATH_REDIRECT
#include <math.h>
#include <fenv_private.h>

TYPE
FUNC (TYPE x)
{
  fenv_t fenv;
  TYPE r;

  libc_feholdexcept_setround_387 (&fenv, FE_OPTION);
  asm volatile ("frndint" : "=t" (r) : "0" (x));
  /* Preserve "invalid" exceptions from sNaN input.  */
  fenv.__status_word |= libc_fetestexcept_387 (FE_INVALID);
  libc_fesetenv_387 (&fenv);

  return r;
}
