/* longjmp.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <pointer_guard.h>
#include <sys/asm.h>

ENTRY (__longjmp)
#ifdef PTR_MANGLE
	REG_L t0, a0, 0*SZREG
	PTR_DEMANGLE (ra, t0, t1)
	REG_L t0, a0, 1*SZREG
	PTR_DEMANGLE2 (sp, t0, t1)
#else
	REG_L ra, a0, 0*SZREG
	REG_L sp, a0, 1*SZREG
#endif
	REG_L x,  a0, 2*SZREG
	REG_L fp, a0, 3*SZREG
	REG_L s0, a0, 4*SZREG
	REG_L s1, a0, 5*SZREG
	REG_L s2, a0, 6*SZREG
	REG_L s3, a0, 7*SZREG
	REG_L s4, a0, 8*SZREG
	REG_L s5, a0, 9*SZREG
	REG_L s6, a0, 10*SZREG
	REG_L s7, a0, 11*SZREG
	REG_L s8, a0, 12*SZREG

#ifndef __loongarch_soft_float
	FREG_L fs0, a0, 13*SZREG + 0*SZFREG
	FREG_L fs1, a0, 13*SZREG + 1*SZFREG
	FREG_L fs2, a0, 13*SZREG + 2*SZFREG
	FREG_L fs3, a0, 13*SZREG + 3*SZFREG
	FREG_L fs4, a0, 13*SZREG + 4*SZFREG
	FREG_L fs5, a0, 13*SZREG + 5*SZFREG
	FREG_L fs6, a0, 13*SZREG + 6*SZFREG
	FREG_L fs7, a0, 13*SZREG + 7*SZFREG
#endif

	sltui	a0, a1, 1
	ADD	a0, a0, a1	 # a0 = (a1 == 0) ? 1 : a1
	jirl	zero, ra, 0

END (__longjmp)
