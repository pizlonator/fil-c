dnl  RISC-V/64 mpn_add_n and mpn_sub_n.

dnl  Copyright 2016 Free Software Foundation, Inc.

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
define(`rp',	`a0')
define(`ap',	`a1')
define(`bp',	`a2')
define(`n',	`a3')

define(`i',	`a6')

ifdef(`OPERATION_add_n',`
    define(`ADDSUB',	`add')
    define(`CMPCY',	`sltu	$1, $2, $3')
    define(`func',	`mpn_add_n')
')
ifdef(`OPERATION_sub_n',`
    define(`ADDSUB',	`sub')
    define(`CMPCY',	`sltu	$1, $3, $4')
    define(`func',	`mpn_sub_n')
')

MULFUNC_PROLOGUE(mpn_add_n mpn_sub_n)

ASM_START()
PROLOGUE(func)
	li	t6, 0
	srli	i, n, 2

	ld	a4, 0(ap)
	ld	a5, 0(bp)

	andi	t0, n, 1
	andi	t1, n, 2
	bnez	t0, L(bx1)
L(bx0):	bnez	t1, L(b10)
L(b00):	addi	rp, rp, -8
	addi	i, i, -1
	j	L(b0)
L(b10):	addi	bp, bp, -16
	addi	ap, ap, -16
	addi	rp, rp, -24
	j	L(b2)
L(bx1):	bnez	t1, L(b11)
L(b01):	beqz	i, L(1)
	addi	bp, bp, 8
	addi	ap, ap, 8
	addi	i, i, -1
	j	L(b1)
L(1):	ADDSUB	t0, a4, a5
	sd	t0, 0(rp)
	CMPCY(	a0, t0, a4, a5)
	ret
L(b11):	addi	bp, bp, -8
	addi	ap, ap, -8
	addi	rp, rp, -16
	j	L(b3)

	ALIGN(	16)
L(top):	addi	bp, bp, 32
	addi	ap, ap, 32
	addi	rp, rp, 32
	addi	i, i, -1
L(b1):	ADDSUB	t0, a4, a5
	CMPCY(	t2, t0, a4, a5)
	ld	a4, 0(ap)
	ld	a5, 0(bp)
	ADDSUB	t4, t0, t6
	CMPCY(	t3, t4, t0, t6)
	sd	t4, 0(rp)
	or	t6, t2, t3

L(b0):	ADDSUB	t1, a4, a5
	CMPCY(	t2, t1, a4, a5)
	ld	a4, 8(ap)
	ld	a5, 8(bp)
	ADDSUB	t4, t1, t6
	CMPCY(	t3, t4, t1, t6)
	sd	t4, 8(rp)
	or	t6, t2, t3
L(b3):	ADDSUB	t0, a4, a5
	CMPCY(	t2, t0, a4, a5)
	ld	a4, 16(ap)
	ld	a5, 16(bp)
	ADDSUB	t4, t0, t6
	CMPCY(	t3, t4, t0, t6)
	sd	t4, 16(rp)
	or	t6, t2, t3
L(b2):	ADDSUB	t1, a4, a5
	CMPCY(	t2, t1, a4, a5)
	ld	a4, 24(ap)
	ld	a5, 24(bp)
	ADDSUB	t4, t1, t6
	CMPCY(	t3, t4, t1, t6)
	sd	t4, 24(rp)
	or	t6, t2, t3
	bne	i, x0, L(top)

L(end):	ADDSUB	t0, a4, a5
	CMPCY(	t2, t0, a4, a5)
	ADDSUB	t4, t0, t6
	CMPCY(	t3, t4, t0, t6)
	sd	t4, 32(rp)
	or	a0, t2, t3
	ret
EPILOGUE()
