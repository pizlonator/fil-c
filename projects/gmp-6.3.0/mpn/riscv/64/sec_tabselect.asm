dnl  RISC-V/64 mpn_sec_tabselect

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
define(`tp',	`a1')
define(`n',	`a2')
define(`nents',	`a3')
define(`which',	`a4')

define(`i',	`a6')
define(`j',	`a7')
define(`mask',	`s0')
define(`k',	`nents')
define(`one',	`s3')

ASM_START()
PROLOGUE(mpn_sec_tabselect)
	addi	sp,sp,-32
	sd	s0,24(sp)
	sd	s1,16(sp)
	sd	s2,8(sp)
	sd	s3,0(sp)

	addi	j, n, -4
	slli	n, n, 3
	li	one, 1

	sub	k, which, nents
	blt	j, zero, L(outer_end)
L(outer_top):
	mv	s2, tp
	li	t0, 0
	li	t1, 0
	li	t2, 0
	li	t3, 0
	addi	j, j, -4
	mv	i, which

	ALIGN(16)
L(top):	ld	t4, 0(tp)
	ld	t5, 8(tp)
	sltu	mask, i, one
	addi	i, i, -1
	neg	mask, mask
	ld	t6, 16(tp)
	ld	a5, 24(tp)
	and	t4, mask, t4
	and	t5, mask, t5
	or	t0, t4, t0
	or	t1, t5, t1
	and	t6, mask, t6
	and	a5, mask, a5
	or	t2, t6, t2
	or	t3, a5, t3
	add	tp, tp, n
	bne	i, k, L(top)

	sd	t0, 0(rp)
	sd	t1, 8(rp)
	sd	t2, 16(rp)
	sd	t3, 24(rp)
	add	tp, s2, 32
	add	rp, rp, 32
	bge	j, zero, L(outer_top)
L(outer_end):
	andi	t0, n, 2*8
	beq	t0, zero, L(b0x)
L(b1x):	mv	s2, tp
	li	t0, 0
	li	t1, 0
	mv	i, which
	ALIGN(16)
L(tp2):	ld	t4, 0(tp)
	ld	t5, 8(tp)
	sltu	mask, i, one
	neg	mask, mask
	addi	i, i, -1
	and	t4, mask, t4
	and	t5, mask, t5
	or	t0, t4, t0
	or	t1, t5, t1
	add	tp, tp, n
	bne	i, k, L(tp2)
	sd	t0, 0(rp)
	sd	t1, 8(rp)
	addi	tp, s2, 16
	addi	rp, rp, 16

L(b0x):	andi	t0, n, 1*8
	beq	t0, zero, L(b00)
L(b01):	li	t0, 0
	mv	i, which
	ALIGN(16)
L(tp1):	ld	t4, 0(tp)
	sltu	mask, i, one
	neg	mask, mask
	addi	i, i, -1
	and	t4, mask, t4
	or	t0, t4, t0
	add	tp, tp, n
	bne	i, k, L(tp1)
	sd	t0, 0(rp)

L(b00):	ld	s0,24(sp)
	ld	s1,16(sp)
	ld	s2,8(sp)
	ld	s3,0(sp)
	addi	sp,sp,32
	jr	ra
EPILOGUE()
