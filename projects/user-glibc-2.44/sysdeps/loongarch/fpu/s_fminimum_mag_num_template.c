/* Return number with minimum magnitude.  LoongArch version.
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
#include_next <s_fminimum_mag_num_template.c>
#else

#include <math.h>

FLOAT
M_DECL_FUNC (__fminimum_mag_num) (FLOAT x, FLOAT y)
{
  FLOAT a, b;
  asm("fcmp.cor." INSN_FMT "\t$fcc0, %2, %2\n\t"
      "fcmp.cor." INSN_FMT "\t$fcc1, %3, %3\n\t"
      "fsel"		   "\t%0, %3, %2, $fcc0\n\t"
      "fsel"		   "\t%1, %2, %3, $fcc1\n\t"
      "fmina."	  INSN_FMT "\t%1, %0, %1"
      : "=&f" (a), "=f" (b) : "f" (x), "f" (y) : "fcc0", "fcc1");
  return b;
}
declare_mgen_alias (__fminimum_mag_num, fminimum_mag_num);

#endif
