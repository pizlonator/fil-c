dnl  RISC-V/64 mpn_rshift

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
define(`rp_arg',`a0')
define(`ap',	`a1')
define(`n',	`a2')
define(`cnt',	`a3')

define(`rp',	`a4')
define(`tnc',	`t5')
define(`i',	`a7')

ASM_START()
PROLOGUE(mpn_rshift)
	mv	rp, rp_arg
	sub	tnc, x0, cnt
	srli	i, n, 2

	ld	t0, 0(ap)
	sll	a0, t0, tnc

	andi	t6, n, 1
	andi	a5, n, 2
	bnez	t6, L(bx1)

	srl	t3, t0, cnt
	ld	t0, 8(ap)
	addi	i, i, -1
	bnez	a5, L(b10)
	addi	rp, rp, -16
	j	L(b0)
L(b10):	addi	ap, ap, 16
	bge	i, x0, L(b2)
L(eq2):	sll	t4, t0, tnc
	srl	t2, t0, cnt
	or	t4, t3, t4
	sd	t4, (rp)
	sd	t2, 8(rp)
	ret

L(bx1):	srl	t2, t0, cnt
	bnez	a5, L(b11)
	bnez	i, L(gt1)
	sd	t2, (rp)
	ret
L(gt1):	ld	t0, 8(ap)
	addi	ap, ap, 8
	addi	rp, rp, -8
	addi	i, i, -1
	j	L(b1)
L(b11):	ld	t0, 8(ap)
	addi	ap, ap, -8
	addi	rp, rp, -24
	j	L(b3)

L(top):	addi	ap, ap, 32
	addi	rp, rp, 32
	addi	i, i, -1
L(b2):	sll	t4, t0, tnc
	srl	t2, t0, cnt
	ld	t0, 0(ap)
	or	t4, t3, t4
	sd	t4, 0(rp)
L(b1):	sll	t4, t0, tnc
	srl	t3, t0, cnt
	ld	t0, 8(ap)
	or	t4, t2, t4
	sd	t4, 8(rp)
L(b0):	sll	t4, t0, tnc
	srl	t2, t0, cnt
	ld	t0, 16(ap)
	or	t4, t3, t4
	sd	t4, 16(rp)
L(b3):	sll	t4, t0, tnc
	srl	t3, t0, cnt
	ld	t0, 24(ap)
	or	t4, t2, t4
	sd	t4, 24(rp)
	bnez	i, L(top)

L(end):	sll	t4, t0, tnc
	srl	t2, t0, cnt
	or	t4, t3, t4
	sd	t4, 32(rp)
	sd	t2, 40(rp)
	ret
EPILOGUE()
