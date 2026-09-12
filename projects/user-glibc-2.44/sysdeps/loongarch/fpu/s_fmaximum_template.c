/* Return maximum of X and Y.  LoongArch version.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#ifndef INSN_FMT
#include_next <s_fmaximum_template.c>
#else

#include <math.h>

FLOAT
M_DECL_FUNC (__fmaximum) (FLOAT x, FLOAT y)
{
  FLOAT a, b;
  asm("fcmp.cor." INSN_FMT "\t$fcc0, %2, %2\n\t"
      "fcmp.cor." INSN_FMT "\t$fcc1, %3, %3\n\t"
      "fsel"		   "\t%0, %2, %3, $fcc0\n\t"
      "fsel"		   "\t%1, %3, %2, $fcc1\n\t"
      "fmax."	  INSN_FMT "\t%1, %0, %1"
      : "=&f" (a), "=f" (b) : "f" (x), "f" (y) : "fcc0", "fcc1");
  return b;
}
declare_mgen_alias (__fmaximum, fmaximum);

#endif
