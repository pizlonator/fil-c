dnl  Loongarch mpn_add_n

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
define(`rp_arg',`$r4')
define(`ap',	`$r5')
define(`bp',	`$r6')
define(`n',	`$r7')

define(`rp',	`$r8')

ASM_START()
PROLOGUE(mpn_add_n)
	alsl.d	rp, n, rp_arg, 3
	alsl.d	ap, n, ap, 3
	alsl.d	bp, n, bp, 3
	sub.d	n, $r0, n
	slli.d	n, n, 3
	or	$r4, $r0, $r0

L(top):	ldx.d	$r14, ap, n
	ldx.d	$r13, bp, n
	add.d	$r12, $r14, $r13
	sltu	$r15, $r12, $r13	C cy0
	add.d	$r14, $r12, $r4
	sltu	$r16, $r14, $r4		C cy1 set iff r4=1 & r12=111...1
	stx.d	$r14, rp, n
	addi.d	n, n, 8
	or	$r4, $r15, $r16
	bnez	n, L(top)

	jr	$r1
EPILOGUE()
