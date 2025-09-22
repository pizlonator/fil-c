dnl  RISC-V/64 mpn_copyd

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

C  INPUT PARAMETERS
define(`rp',	`a0')
define(`ap',	`a1')
define(`n',	`a2')

define(`i',	`a6')

ASM_START()
PROLOGUE(mpn_copyd)
	slli	t0, n, 3
	add	ap, ap, t0
	add	rp, rp, t0
	srli	i, n, 2

	andi	t0, n, 1
	andi	t1, n, 2
	bnez	t0, L(bx1)
L(bx0):	beqz	n, L(ret)
	ld	t0, -8(ap)
	bnez	t1, L(b10)
L(b00):	addi	rp, rp, 8
	addi	i, i, -1
	j	L(b0)
L(b10):	addi	ap, ap, 16
	addi	rp, rp, 24
	j	L(b2)
L(bx1):	ld	t2, -8(ap)
	bnez	t1, L(b11)
	beqz	i, L(1)
	addi	ap, ap, -8
	addi	i, i, -1
	j	L(b1)
L(1):	sd	t2, -8(rp)
	ret
L(b11):	addi	ap, ap, 8
	addi	rp, rp, 16
	j	L(b3)

	ALIGN(	16)
L(top):	addi	ap, ap, -32
	addi	rp, rp, -32
	addi	i, i, -1
L(b1):	ld	t0, -8(ap)
	sd	t2, -8(rp)
L(b0):	ld	t2, -16(ap)
	sd	t0, -16(rp)
L(b3):	ld	t0, -24(ap)
	sd	t2, -24(rp)
L(b2):	ld	t2, -32(ap)
	sd	t0, -32(rp)
	bnez	i, L(top)

L(end):	sd	t2, -40(rp)
L(ret):	ret
EPILOGUE()
