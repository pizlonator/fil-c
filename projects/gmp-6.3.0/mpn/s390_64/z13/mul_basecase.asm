dnl  S/390-64 mpn_mul_basecase.

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


C INPUT PARAMETERS
define(`rp',	`%r2')
define(`ap',	`%r3')
define(`an',	`%r4')	C 32
define(`bp',	`%r5')	C 40
define(`bn',	`%r6')	C 48

define(`idx',	`%r14')
define(`b0',	`%r10')

dnl live in addmul_1:
dnl r0  r1  r2  r3  r4  r5  r6  r7  r8  r9 r10 r11 r12 r13 r14
dnl xx  xx  rp  ap  an  bp  xx  xx  xx  xx  b0  i   xx  xx idx
dnl stack: bn

dnl TODO
dnl  * Have mul_1 start without initial (un mod 4) separation, instead handle
dnl    after loop.  Then fall into 4 separate addmul_1 loops.
dnl  * Streamline handling of bn, an, %r11 to reduce the # if memops.

define(`MUL_1',`
pushdef(`L',
defn(`L')$1`'_m1)
	vzero	%v2
	srlg	%r11, %r0, 2

	tmll	%r0, 1
	je	L(bx0)
L(bx1):	tmll	%r0, 2
	jne	L(b11)

L(b01):	lghi	idx, -24
	lg	%r13, 0(ap)
	mlgr	%r12, b0
	stg	%r13, 0(rp)
	cgijne	%r11, 0, L(cj0)

L(1):	stg	%r12, 8(rp)
	lmg	%r6, %r14, 48(%r15)
	br	%r14

L(b11):	lghi	idx, -8
	lg	%r9, 0(ap)
	mlgr	%r8, b0
	stg	%r9, 0(rp)
	j	L(cj1)

L(bx0):	tmll	%r0, 2
	jne	L(b10)
L(b00):	lghi	idx, -32
	lghi	%r12, 0
L(cj0):	lg	%r1, 32(idx, ap)
	lg	%r9, 40(idx, ap)
	mlgr	%r0, b0
	mlgr	%r8, b0
	vlvgp	%v6, %r0, %r1
	vlvgp	%v7, %r9, %r12
	j	L(mid)

L(b10):	lghi	idx, -16
	lghi	%r8, 0
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
	vacq	%v3, %v6, %v7, %v2
	vacccq	%v2, %v6, %v7, %v2
	vpdi	%v3, %v3, %v3, 4
	vst	%v3, 16(idx, rp), 3
	vlvgp	%v6, %r0, %r1
	vlvgp	%v7, %r9, %r12
L(mid):	lg	%r7, 48(idx, ap)
	lg	%r13, 56(idx, ap)
	mlgr	%r6, b0
	mlgr	%r12, b0
	vacq	%v1, %v6, %v7, %v2
	vacccq	%v2, %v6, %v7, %v2
	vpdi	%v1, %v1, %v1, 4
	vst	%v1, 32(idx, rp), 3
	vlvgp	%v6, %r6, %r7
	vlvgp	%v7, %r13, %r8
	la	idx, 32(idx)
	brctg	%r11, L(top)

L(end):	vacq	%v3, %v6, %v7, %v2
	vacccq	%v2, %v6, %v7, %v2
	vpdi	%v3, %v3, %v3, 4
	vst	%v3, 16(idx, rp), 3

	vlgvg	%r0, %v2, 1
	algr	%r0, %r12
	stg	%r0, 32(idx, rp)
popdef(`L')
')

define(`ADDMUL_1',`
pushdef(`L',
defn(`L')$1`'_am1)
	vzero	%v0
	vzero	%v2
	srlg	%r11, %r0, 2

	tmll	%r0, 1
	je	L(bx0)
L(bx1):	tmll	%r0, 2
	jne	L(b11)

L(b01):	lghi	idx, -24
	vleg	%v2, 0(rp), 1
	lg	%r13, 0(ap)
	vzero	%v4
	mlgr	%r12, b0
	vlvgg	%v4, %r13, 1
	vaq	%v2, %v2, %v4
	vsteg	%v2, 0(rp), 1
	vmrhg	%v2, %v2, %v2
	j	L(cj0)

L(b11):	lghi	idx, -8
	vleg	%v2, 0(rp), 1
	lg	%r9, 0(ap)
	vzero	%v4
	mlgr	%r8, b0
	vlvgg	%v4, %r9, 1
	vaq	%v2, %v2, %v4
	vsteg	%v2, 0(rp), 1
	vmrhg	%v2, %v2, %v2
	j	L(cj1)

L(bx0):	tmll	%r0, 2
	jne	L(b10)
L(b00):	lghi	idx, -32
	lghi	%r12, 0
L(cj0):	lg	%r1, 32(idx, ap)
	lg	%r9, 40(idx, ap)
	mlgr	%r0, b0
	mlgr	%r8, b0
	vlvgp	%v6, %r0, %r1
	vlvgp	%v7, %r9, %r12
	j	L(mid)

L(b10):	lghi	idx, -16
	lghi	%r8, 0
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
	vlgvg	%r0, %v2, 1
	algr	%r0, %r12
	stg	%r0, 32(idx, rp)
popdef(`L')
')


ASM_START()

PROLOGUE(mpn_mul_basecase)
	stmg	%r4, %r14, 32(%r15)

	lgr	%r4, bn

	lg	%r0, 32(%r15)
	lg	b0, 0(bp)
	MUL_1()			C implicitly pass r0 = an

	aghi	%r4, -1
	je	L(end)
L(top):	lg	%r0, 32(%r15)
	la	bp, 8(bp)
	la	rp, 8(rp)
	lg	b0, 0(bp)
	ADDMUL_1()		C implicitly pass r0 = an
	brctg	%r4, L(top)

L(end):	lmg	%r6, %r14, 48(%r15)
	br	%r14
EPILOGUE()
	.section	.note.GNU-stack
