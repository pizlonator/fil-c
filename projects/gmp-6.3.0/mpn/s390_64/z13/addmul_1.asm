dnl  S/390-64 mpn_addmul_1 and mpn_addmul_1c.
dnl  Based on C code contributed by Marius Hillenbrand.

dnl  Copyright 2023 Free Software Foundation, Inc.

dnl  This file is part of the GNU MP Library.
dnl
dnl  The GNU MP Library is free software; you can redistribute it and/or modify
dnl  it under the terms of either:
dnl
dnl    * the GNU Lesser General Public License as published by the Free
dnl      Software Foundation; either version 3 of the License, or (at your
dnl      option) any later version.
dnl
dnl  or
dnl
dnl    * the GNU General Public License as published by the Free Software
dnl      Foundation; either version 2 of the License, or (at your option) any
dnl      later version.
dnl
dnl  or both in parallel, as here.
dnl
dnl  The GNU MP Library is distributed in the hope that it will be useful, but
dnl  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
dnl  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
dnl  for more details.
dnl
dnl  You should have received copies of the GNU General Public License and the
dnl  GNU Lesser General Public License along with the GNU MP Library.  If not,
dnl  see https://www.gnu.org/licenses/.

include(`../config.m4')

dnl TODO
dnl * Schedule vlvgp away from mlgr; that saves 20% of the run time.
dnl * Perhaps use vp[0]/vp[1] in innerloop instead preloading v0/v1.

C            cycles/limb
C z900		 -
C z990		 -
C z9		 -
C z10		 -
C z196		 -
C z12		 ?
C z13		 ?
C z14		 ?
C z15		 2.55


define(`rp',	`%r2')
define(`ap',	`%r3')
define(`an',	`%r4')
define(`b0',	`%r5')
define(`cy',	`%r6')

define(`idx',	`%r4')

ASM_START()

PROLOGUE(mpn_addmul_1c)
	stmg	%r6, %r13, 48(%r15)
	j	L(ent)
EPILOGUE()

PROLOGUE(mpn_addmul_1)
	stmg	%r6, %r13, 48(%r15)
	lghi	%r6, 0
L(ent):	vzero	%v0
	vzero	%v2
	srlg	%r11, an, 2

	tmll	an, 1
	je	L(bx0)
L(bx1):	tmll	an, 2
	jne	L(b11)

L(b01):	lghi	idx, -24
	vleg	%v2, 0(rp), 1
	lg	%r13, 0(ap)
	vzero	%v4
	mlgr	%r12, b0
	algr	%r13, %r6
	lghi	%r6, 0
	alcgr	%r12, %r6
	vlvgg	%v4, %r13, 1
	vaq	%v2, %v2, %v4
	vsteg	%v2, 0(rp), 1
	vmrhg	%v2, %v2, %v2
	cgije	%r11, 0, L(1)
	j	L(cj0)

L(b11):	lghi	idx, -8
	vleg	%v2, 0(rp), 1
	lg	%r9, 0(ap)
	vzero	%v4
	mlgr	%r8, b0
	algr	%r9, %r6
	lghi	%r6, 0
	alcgr	%r8, %r6
	vlvgg	%v4, %r9, 1
	vaq	%v2, %v2, %v4
	vsteg	%v2, 0(rp), 1
	vmrhg	%v2, %v2, %v2
	j	L(cj1)

L(bx0):	tmll	an, 2
	jne	L(b10)
L(b00):	lghi	idx, -32
	lgr	%r12, %r6
L(cj0):	lg	%r1, 32(idx, ap)
	lg	%r9, 40(idx, ap)
	mlgr	%r0, b0
	mlgr	%r8, b0
	vlvgp	%v6, %r0, %r1
	vlvgp	%v7, %r9, %r12
	j	L(mid)

L(b10):	lghi	idx, -16
	lgr	%r8, %r6
L(cj1):	lg	%r7, 16(idx, ap)
	lg	%r13, 24(idx, ap)
	mlgr	%r6, b0
	mlgr	%r12, b0
	vlvgp	%v6, %r6, %r7
	vlvgp	%v7, %r13, %r8
	cgije	%r11, 0, L(end)

L(top):	lg	%r1, 32(idx, ap)
	lg	%r9, 40(idx, ap)
	mlgr	%r0, b0
	mlgr	%r8, b0
	vl	%v1, 16(idx, rp), 3
	vpdi	%v1, %v1, %v1, 4
	vacq	%v5, %v6, %v1, %v0
	vacccq	%v0, %v6, %v1, %v0
	vacq	%v3, %v5, %v7, %v2
	vacccq	%v2, %v5, %v7, %v2
	vpdi	%v3, %v3, %v3, 4
	vst	%v3, 16(idx, rp), 3
	vlvgp	%v6, %r0, %r1
	vlvgp	%v7, %r9, %r12
L(mid):	lg	%r7, 48(idx, ap)
	lg	%r13, 56(idx, ap)
	mlgr	%r6, b0
	mlgr	%r12, b0
	vl	%v4, 32(idx, rp), 3
	vpdi	%v4, %v4, %v4, 4
	vacq	%v5, %v6, %v4, %v0
	vacccq	%v0, %v6, %v4, %v0
	vacq	%v1, %v5, %v7, %v2
	vacccq	%v2, %v5, %v7, %v2
	vpdi	%v1, %v1, %v1, 4
	vst	%v1, 32(idx, rp), 3
	vlvgp	%v6, %r6, %r7
	vlvgp	%v7, %r13, %r8
	la	idx, 32(idx)
	brctg	%r11, L(top)

L(end):	vl	%v1, 16(idx, rp), 3
	vpdi	%v1, %v1, %v1, 4
	vacq	%v5, %v6, %v1, %v0
	vacccq	%v0, %v6, %v1, %v0
	vacq	%v3, %v5, %v7, %v2
	vacccq	%v2, %v5, %v7, %v2
	vpdi	%v3, %v3, %v3, 4
	vst	%v3, 16(idx, rp), 3

	vag	%v2, %v0, %v2
L(1):	vlgvg	%r2, %v2, 1
	algr	%r2, %r12
	lmg	%r6, %r13, 48(%r15)
	br	%r14
EPILOGUE()
