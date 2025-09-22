dnl  S/390-64 mpn_sec_tabselect

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

C            cycles/limb
C z900		 ?
C z990		 ?
C z9		 ?
C z10		 ?
C z196		 ?
C z13		 ?
C z14		 ?
C z15		 1.6

dnl void
dnl mpn_sec_tabselect (volatile mp_limb_t *rp, volatile const mp_limb_t *tab,
dnl                    mp_size_t n, mp_size_t nents, mp_size_t which)

define(`rp',	`%r2')
define(`tp',	`%r3')
define(`n',	`%r4')
define(`nents',	`%r5')
define(`which_arg',`%r6')	C magicked to stack

dnl    r0    r1    r2    r3    r4    r5    r6    r7
dnl    r8    r9    r10   r11   r12   r13   r14   r15

define(`mask',	`%r14')
define(`k',	`%r1')
define(`which',	`%r0')

define(`FRAME', 64)

ASM_START()
PROLOGUE(mpn_sec_tabselect)
	stmg	%r5, %r15, 40(%r15)
	aghi	%r15, -FRAME

	sllg	n, n, 3
	msgr	%r5, n
	stg	%r5, 16(%r15)			C nents * n * LIMB_BYTES

	srlg	%r5, n, 2+3
	ngr	%r5, %r5
	je	L(end4)
L(outer):
	lg	which, eval(48+FRAME)(%r15)
	lg	k, eval(40+FRAME)(%r15)		C nents
	lghi	%r6, 0
	lghi	%r7, 0
	lghi	%r8, 0
	lghi	%r9, 0
L(tp4):	lghi	mask, 1
	slgr	which, mask
	slbgr	mask, mask
	lmg	%r10, %r13, 0(tp)
	ngr	%r10, mask
	ngr	%r11, mask
	ngr	%r12, mask
	ngr	%r13, mask
	agr	%r6, %r10
	agr	%r7, %r11
	agr	%r8, %r12
	agr	%r9, %r13
	agr	tp, n
	brctg	k, L(tp4)
	stmg	%r6, %r9, 0(rp)
	aghi	rp, 32
	slg	tp, 16(%r15)
	aghi	tp, eval(4*8)
	brctg	%r5, L(outer)
L(end4):
	tmll	n, 16
	je	L(end2)
	lg	which, eval(48+FRAME)(%r15)
	lg	k, eval(40+FRAME)(%r15)		C nents
	lghi	%r6, 0
	lghi	%r7, 0
L(tp2):	lghi	mask, 1
	slgr	which, mask
	slbgr	mask, mask
	lmg	%r10, %r11, 0(tp)
	ngr	%r10, mask
	ngr	%r11, mask
	agr	%r6, %r10
	agr	%r7, %r11
	agr	tp, n
	brctg	k, L(tp2)
	stmg	%r6, %r7, 0(rp)
	aghi	rp, 16
	slg	tp, 16(%r15)
	aghi	tp, eval(2*8)
L(end2):
	tmll	n, 8
	je	L(end1)
	lg	which, eval(48+FRAME)(%r15)
	lg	k, eval(40+FRAME)(%r15)		C nents
	lghi	%r6, 0
L(tp1):	lghi	mask, 1
	slgr	which, mask
	slbgr	mask, mask
	lg	%r10, 0(tp)
	ngr	%r10, mask
	agr	%r6, %r10
	agr	tp, n
	brctg	k, L(tp1)
	stg	%r6, 0(rp)
L(end1):
	lmg	%r5, %r15, eval(40+FRAME)(%r15)
	br	%r14
EPILOGUE()
