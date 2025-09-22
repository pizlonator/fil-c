dnl  Loongarch mpn_addlshC_n/mpn_sublshC_n

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


define(`rp',`$a0')
define(`ap',`$a1')
define(`bp',`$a2')
define(`n', `$a3')

define(`i', `$a4')


ASM_START()
PROLOGUE(func)
	srli.d	i, n, 2
	move	$a5, $zero
	move	$t7, $zero

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
	ld.d	$t0, bp, 0
	alsl.d	$t6, $t0, $t7, LSH
	ld.d	$t2, ap, 0
	ADDSUB	$t4, $t2, $t6
	CARRY(	$a6, $t4, $t2, $t6)
	srli.d	$t7, $t0, RSH
	ADDSUB	$t5, $t4, $a5
	CARRY(	$a5, $t5, $t4, $a5)
	st.d	$t5, rp, 0
	or	$a5, $a5, $a6
L(b3):	ld.d	$t0, bp, 8
	alsl.d	$t6, $t0, $t7, LSH
	ld.d	$t2, ap, 8
	ADDSUB	$t4, $t2, $t6
	CARRY(	$a6, $t4, $t2, $t6)
	srli.d	$t7, $t0, RSH
	ADDSUB	$t5, $t4, $a5
	CARRY(	$a5, $t5, $t4, $a5)
	st.d	$t5, rp, 8
	or	$a5, $a5, $a6
L(b2):	ld.d	$t0, bp, 16
	alsl.d	$t6, $t0, $t7, LSH
	ld.d	$t2, ap, 16
	ADDSUB	$t4, $t2, $t6
	CARRY(	$a6, $t4, $t2, $t6)
	srli.d	$t7, $t0, RSH
	ADDSUB	$t5, $t4, $a5
	CARRY(	$a5, $t5, $t4, $a5)
	st.d	$t5, rp, 16
	or	$a5, $a5, $a6
L(b1):	ld.d	$t0, bp, 24
	alsl.d	$t6, $t0, $t7, LSH
	ld.d	$t2, ap, 24
	ADDSUB	$t4, $t2, $t6
	CARRY(	$a6, $t4, $t2, $t6)
	srli.d	$t7, $t0, RSH
	ADDSUB	$t5, $t4, $a5
	CARRY(	$a5, $t5, $t4, $a5)
	st.d	$t5, rp, 24
	or	$a5, $a5, $a6
	bnez	i, L(top)

L(end):	add.d	$a0, $a5, $t7
	jr	$r1
EPILOGUE()
