/* setjmp for LoongArch.
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

ENTRY (_setjmp)
	li.w		a1,0
	b		HIDDEN_JUMPTARGET (__sigsetjmp)
END (_setjmp)

ENTRY (setjmp)
	li.w		a1,1
END (setjmp)

ENTRY (__sigsetjmp)
#ifdef PTR_MANGLE
	PTR_MANGLE (t0, ra, t1)
	REG_S t0, a0, 0*SZREG
	PTR_MANGLE2 (t0, sp, t1)
	REG_S t0, a0, 1*SZREG
#else
	REG_S ra, a0, 0*SZREG
	REG_S sp, a0, 1*SZREG
#endif
	REG_S x,  a0, 2*SZREG
	REG_S fp, a0, 3*SZREG
	REG_S s0, a0, 4*SZREG
	REG_S s1, a0, 5*SZREG
	REG_S s2, a0, 6*SZREG
	REG_S s3, a0, 7*SZREG
	REG_S s4, a0, 8*SZREG
	REG_S s5, a0, 9*SZREG
	REG_S s6, a0, 10*SZREG
	REG_S s7, a0, 11*SZREG
	REG_S s8, a0, 12*SZREG

#ifndef __loongarch_soft_float
	FREG_S fs0, a0, 13*SZREG + 0*SZFREG
	FREG_S fs1, a0, 13*SZREG + 1*SZFREG
	FREG_S fs2, a0, 13*SZREG + 2*SZFREG
	FREG_S fs3, a0, 13*SZREG + 3*SZFREG
	FREG_S fs4, a0, 13*SZREG + 4*SZFREG
	FREG_S fs5, a0, 13*SZREG + 5*SZFREG
	FREG_S fs6, a0, 13*SZREG + 6*SZFREG
	FREG_S fs7, a0, 13*SZREG + 7*SZFREG
#endif

#if !IS_IN (libc) && IS_IN(rtld)
	li.w		v0, 0
	jirl		zero, ra, 0
#else
	b		__sigjmp_save
#endif
END (__sigsetjmp)

hidden_def (__sigsetjmp)
weak_alias (_setjmp, __GI__setjmp)
