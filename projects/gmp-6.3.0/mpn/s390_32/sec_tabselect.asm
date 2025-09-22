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
C z15		 ?

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

define(`FRAME', 32)

ASM_START()
PROLOGUE(mpn_sec_tabselect)
	stm	%r5, %r15, 20(%r15)
	ahi	%r15, -FRAME

	sll	n, 2
	msr	%r5, n
	st	%r5, 16(%r15)			C nents * n * LIMB_BYTES

	lr	%r5, n
	srl	%r5, 2+2
	nr	%r5, %r5
	je	L(end4)
L(outer):
	l	which, eval(24+FRAME)(%r15)
	l	k, eval(20+FRAME)(%r15)		C nents
	lhi	%r6, 0
	lhi	%r7, 0
	lhi	%r8, 0
	lhi	%r9, 0
L(tp4):	lhi	mask, 1
	slr	which, mask
	slbr	mask, mask
	lm	%r10, %r13, 0(tp)
	nr	%r10, mask
	nr	%r11, mask
	nr	%r12, mask
	nr	%r13, mask
	ar	%r6, %r10
	ar	%r7, %r11
	ar	%r8, %r12
	ar	%r9, %r13
	ar	tp, n
	brct	k, L(tp4)
	stm	%r6, %r9, 0(rp)
	ahi	rp, 16
	sl	tp, 16(%r15)
	ahi	tp, eval(4*4)
	brct	%r5, L(outer)
L(end4):
	tmll	n, 8
	je	L(end2)
	l	which, eval(24+FRAME)(%r15)
	l	k, eval(20+FRAME)(%r15)		C nents
	lhi	%r6, 0
	lhi	%r7, 0
L(tp2):	lhi	mask, 1
	slr	which, mask
	slbr	mask, mask
	lm	%r10, %r11, 0(tp)
	nr	%r10, mask
	nr	%r11, mask
	ar	%r6, %r10
	ar	%r7, %r11
	ar	tp, n
	brct	k, L(tp2)
	stm	%r6, %r7, 0(rp)
	ahi	rp, 8
	sl	tp, 16(%r15)
	ahi	tp, eval(2*4)
L(end2):
	tmll	n, 4
	je	L(end1)
	l	which, eval(24+FRAME)(%r15)
	l	k, eval(20+FRAME)(%r15)		C nents
	lhi	%r6, 0
L(tp1):	lhi	mask, 1
	slr	which, mask
	slbr	mask, mask
	l	%r10, 0(tp)
	nr	%r10, mask
	ar	%r6, %r10
	ar	tp, n
	brct	k, L(tp1)
	st	%r6, 0(rp)
L(end1):
	lm	%r5, %r15, eval(20+FRAME)(%r15)
	br	%r14
EPILOGUE()
