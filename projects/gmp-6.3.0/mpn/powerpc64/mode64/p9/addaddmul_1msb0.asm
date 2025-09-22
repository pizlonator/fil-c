dnl  Power9 mpn_addaddmul_1msb0

dnl  Copyright 2021 Free Software Foundation, Inc.

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

C        cycles/limb
C		1-way	2-way	4-way	8-way	16-way	mul_1+addmul_1
C power9:    	 4.55	 3.87	 3.55	 3.35	 3.25	   5.16

C TODO
C  * Only WAYS = 4 currently has proper feed-in code.
C  * Try ldu/stdu to save the explicit updates.
C  * Try using madd in a long dependent chain, only breaking the recurrency
C    once per iteration.
C  * Some cycles could perhaps be saved by scheduling the crX-setting insns.

define(`rp', r3)
define(`ap', r4)
define(`bp', r5)
define(`n',  r6)
define(`u0', r7)
define(`v0', r8)

define(`BLOCK',`
L(lo`'eval((WAYS-$1)%4)):
	ld	r10, eval(8*$1)(ap)
	ld	r11, eval(8*$1)(bp)
	mulld	r12, r10, u0
	mulhdu	r10, r10, u0
	maddld(	r6, r11, v0, r12)
	maddhdu(r11, r11, v0, r12)
	adde	r12, r6, r0
	std	r12, eval(8*$1)(rp)
	add	r0, r10, r11')	

ifdef(`WAYS',,`define(`WAYS',4)')

PROLOGUE(mpn_addaddmul_1msb0)
	addi	r10, n, WAYS-1
	srdi	r10, r10, m4_log2(WAYS)
	mtctr	r10
	addic	r0, r3, 0
	li	r0, 0
ifelse(WAYS,4,`
	rldicl.	r9, n, 0, 63
	rldicl	r10, n, 63, 63
	cmpdi	cr7, r10, 0
	bne	cr0, L(bx1)

L(bx0):	beq	cr7, L(lo0)

L(b10):	addi	ap, ap, -16
	addi	bp, bp, -16
	addi	rp, rp, -16
	b	L(lo2)

L(bx1):	bne	cr7, L(b11)

L(b01):	addi	ap, ap, -24
	addi	bp, bp, -24
	addi	rp, rp, -24
	b	L(lo1)

L(b11):	addi	ap, ap, -8
	addi	bp, bp, -8
	addi	rp, rp, -8
	b	L(lo3)
')

L(top):	forloop(i,0,eval(WAYS-1),`BLOCK(i)')

	addi	ap, ap, eval(8*WAYS)
	addi	bp, bp, eval(8*WAYS)
	addi	rp, rp, eval(8*WAYS)
	bdnz	L(top)

	addze	r3, r0
	blr
EPILOGUE()
