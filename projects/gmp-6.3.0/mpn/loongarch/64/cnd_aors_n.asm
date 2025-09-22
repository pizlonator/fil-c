dnl  Loongarch mpn_cnd_add_n and mpn_cnd_sub_n.

dnl  Contributed to the GNU project by Torbjorn Granlund.

dnl  Copyright 2016, 2023 Free Software Foundation, Inc.

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

C  INPUT PARAMETERS
define(`cnd',	`$a0')
define(`rp',	`$a1')
define(`up',	`$a2')
define(`vp',	`$a3')
define(`n',	`$a4')

define(`mask',	`$t5')

ifdef(`OPERATION_cnd_add_n',`
    define(`ADDSUB',	`add.d')
    define(`CMPCY',	`sltu	$1, $2, $3')
    define(`func',	`mpn_cnd_add_n')
')
ifdef(`OPERATION_cnd_sub_n',`
    define(`ADDSUB',	`sub.d')
    define(`CMPCY',	`sltu	$1, $3, $4')
    define(`func',	`mpn_cnd_sub_n')
')

MULFUNC_PROLOGUE(mpn_cnd_add_n mpn_cnd_sub_n)

ASM_START()
PROLOGUE(func)
	move	$t6, $zero

	sltui	mask, cnd, 1
	addi.d	mask, mask, -1

	andi	$t0, n, 1
	beqz	$t0, L(top)
	addi.d	up, up, 8
	addi.d	vp, vp, -8
	addi.d	rp, rp, -8
	addi.d	n, n, -1
	b	L(mid)

L(top):	ld.d	$a7, vp, 0
	ld.d	$a5, up, 0
	addi.d	n, n, -2		C bookkeeping
	addi.d	up, up, 16		C bookkeeping
	and	$a7, $a7, mask
	ADDSUB	$t0, $a5, $a7
	CMPCY(	$t2, $t0, $a5, $a7)
	ADDSUB	$t4, $t0, $t6		C cycle 3, 9, ...
	CMPCY(	$t3, $t4, $t0, $t6)	C cycle 4, 10, ...
	st.d	$t4, rp, 0
	add.d	$t6, $t2, $t3		C cycle 5, 11, ...
L(mid):	ld.d	$a7, vp, 8
	ld.d	$a5, up, -8
	addi.d	vp, vp, 16		C bookkeeping
	addi.d	rp, rp, 16		C bookkeeping
	and	$a7, $a7, mask
	ADDSUB	$t1, $a5, $a7
	CMPCY(	$t2, $t1, $a5, $a7)
	ADDSUB	$t4, $t1, $t6		C cycle 0, 6, ...
	CMPCY(	$t3, $t4, $t1, $t6)	C cycle 1, 7, ...
	st.d	$t4, rp, -8
	add.d	$t6, $t2, $t3		C cycle 2, 8, ...
	bnez	n, L(top)

L(end):	move	$a0, $t6
	jr	$r1
EPILOGUE()
ASM_END()
