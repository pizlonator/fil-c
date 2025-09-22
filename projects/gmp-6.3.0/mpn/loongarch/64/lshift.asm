dnl  Loongarch mpn_lshift

dnl  Contributed to the GNU project by Torbjorn Granlund.

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
define(`rp_arg',`$a0')
define(`ap',	`$a1')
define(`n',	`$a2')
define(`cnt',	`$a3')

define(`rp',	`$a4')
define(`tnc',	`$t8')
define(`i',	`$a7')

ASM_START()
PROLOGUE(mpn_lshift)
	alsl.d	ap, n, ap, 3
	alsl.d	rp, n, rp_arg, 3
	sub.d	tnc, $zero, cnt
	srli.d	i, n, 2

	ld.d	$t0, ap, -8
	srl.d	$a0, $t0, tnc

	andi	$t6, n, 1
	andi	$t7, n, 2
	bnez	$t6, L(bx1)

	sll.d	$t3, $t0, cnt
	ld.d	$t0, ap, -16
	addi.d	i, i, -1
	bnez	$t7, L(b10)
	addi.d	rp, rp, 16
	b	L(b0)
L(b10):	addi.d	ap, ap, -16
	bge	i, $zero, L(b2)
L(eq2):	srl.d	$t4, $t0, tnc
	sll.d	$t2, $t0, cnt
	or	$t4, $t3, $t4
	st.d	$t4, rp, -8
	st.d	$t2, rp, -16
	jr	$r1

L(bx1):	sll.d	$t2, $t0, cnt
	bnez	$t7, L(b11)
	bnez	i, L(gt1)
	st.d	$t2, rp, -8
	jr	$r1
L(gt1):	ld.d	$t0, ap, -16
	addi.d	ap, ap, -8
	addi.d	rp, rp, 8
	addi.d	i, i, -1
	b	L(b1)
L(b11):	ld.d	$t0, ap, -16
	addi.d	ap, ap, 8
	addi.d	rp, rp, 24
	b	L(b3)

L(top):	addi.d	ap, ap, -32
	addi.d	rp, rp, -32
	addi.d	i, i, -1
L(b2):	srl.d	$t4, $t0, tnc
	sll.d	$t2, $t0, cnt
	ld.d	$t0, ap, -8
	or	$t4, $t3, $t4
	st.d	$t4, rp, -8
L(b1):	srl.d	$t4, $t0, tnc
	sll.d	$t3, $t0, cnt
	ld.d	$t0, ap, -16
	or	$t4, $t2, $t4
	st.d	$t4, rp, -16
L(b0):	srl.d	$t4, $t0, tnc
	sll.d	$t2, $t0, cnt
	ld.d	$t0, ap, -24
	or	$t4, $t3, $t4
	st.d	$t4, rp, -24
L(b3):	srl.d	$t4, $t0, tnc
	sll.d	$t3, $t0, cnt
	ld.d	$t0, ap, -32
	or	$t4, $t2, $t4
	st.d	$t4, rp, -32
	bnez	i, L(top)

L(end):	srl.d	$t4, $t0, tnc
	sll.d	$t2, $t0, cnt
	or	$t4, $t3, $t4
	st.d	$t4, rp, -40
	st.d	$t2, rp, -48
	jr	$r1
EPILOGUE()
