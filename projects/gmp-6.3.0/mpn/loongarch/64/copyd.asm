dnl  Loongarch mpn_copyd

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
define(`n',	`$a2')

define(`i',	`$a3')

ASM_START()
PROLOGUE(mpn_copyd)
	alsl.d	ap, n, ap, 3
	alsl.d	rp, n, rp, 3
	srli.d	i, n, 2
	beqz	i, L(end)

L(top):	addi.d	i, i, -1
	ld.d	$t0, ap, -8
	st.d	$t0, rp, -8
	ld.d	$t1, ap, -16
	st.d	$t1, rp, -16
	ld.d	$t2, ap, -24
	st.d	$t2, rp, -24
	ld.d	$t3, ap, -32
	st.d	$t3, rp, -32
	addi.d	ap, ap, -32
	addi.d	rp, rp, -32
	bnez	i, L(top)

L(end):	andi	$t1, n, 2
	beqz	$t1, L(b0x)
	ld.d	$t0, ap, -8
	st.d	$t0, rp, -8
	ld.d	$t1, ap, -16
	st.d	$t1, rp, -16
	addi.d	ap, ap, -16
	addi.d	rp, rp, -16
L(b0x):	andi	$t0, n, 1
	beqz	$t0, L(bx0)
	ld.d	$t0, ap, -8
	st.d	$t0, rp, -8
L(bx0):	jr	$r1
EPILOGUE()
