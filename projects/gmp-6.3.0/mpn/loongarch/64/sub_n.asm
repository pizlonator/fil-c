dnl  Loongarch mpn_sub_n

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
define(`rp',	`$a0')
define(`ap',	`$a1')
define(`bp',	`$a2')
define(`n',	`$a3')

define(`i',	`$a7')

ASM_START()
PROLOGUE(mpn_sub_n)
	srli.d	i, n, 2
	move	$t8, $zero

	andi	$t0, n, 1
	andi	$t1, n, 2
	bnez	$t0, L(bx1)
L(bx0):	beqz	$t1, L(b0)
L(b10):	addi.d	bp, bp, -16
	addi.d	ap, ap, -16
	addi.d	rp, rp, -16
	b	L(b2)
L(bx1):	bnez	$t1, L(b11)
L(b01):	addi.d	bp, bp, -24
	addi.d	ap, ap, -24
	addi.d	rp, rp, -24
	b	L(b1)
L(b11):	addi.d	bp, bp, -8
	addi.d	ap, ap, -8
	addi.d	rp, rp, -8
	b	L(b3)

L(top):	addi.d	bp, bp, 32
	addi.d	ap, ap, 32
	addi.d	rp, rp, 32
L(b0):	addi.d	i, i, -1
	ld.d	$t4, bp, 0
	ld.d	$t0, ap, 0
	sltu	$a4, $t0, $t4
	sub.d	$t0, $t0, $t4
	sltu	$a5, $t0, $t8		C 0
	sub.d	$t0, $t0, $t8		C 0
	or	$t8, $a4, $a5		C 1
	st.d	$t0, rp, 0
L(b3):	ld.d	$t5, bp, 8
	ld.d	$t1, ap, 8
	sltu	$a4, $t1, $t5
	sub.d	$t1, $t1, $t5
	sltu	$a5, $t1, $t8		C 2
	sub.d	$t1, $t1, $t8		C 2
	or	$t8, $a4, $a5		C 3
	st.d	$t1, rp, 8
L(b2):	ld.d	$t4, bp, 16
	ld.d	$t0, ap, 16
	sltu	$a4, $t0, $t4
	sub.d	$t0, $t0, $t4
	sltu	$a5, $t0, $t8		C 4
	sub.d	$t0, $t0, $t8		C 4
	or	$t8, $a4, $a5		C 5
	st.d	$t0, rp, 16
L(b1):	ld.d	$t5, bp, 24
	ld.d	$t1, ap, 24
	sltu	$a4, $t1, $t5
	sub.d	$t1, $t1, $t5
	sltu	$a5, $t1, $t8		C 6
	sub.d	$t1, $t1, $t8		C 6
	or	$t8, $a4, $a5		C 7
	st.d	$t1, rp, 24
	bnez	i, L(top)

	move	$a0, $t8
	jr	$r1
EPILOGUE()
