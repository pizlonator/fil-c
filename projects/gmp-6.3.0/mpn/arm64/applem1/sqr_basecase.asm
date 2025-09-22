dnl  ARM64 mpn_sqr_basecase

dnl  Contributed to the GNU project by Torbjörn Granlund.

dnl  Copyright 2020 Free Software Foundation, Inc.

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

dnl TODO
dnl  * Replace the mul_1 code with less scheduled and thus simpler code.  If
dnl    we base it on the addmul_1 loop, the corner code could benefit from
dnl    similar incoming register state, which could eliminate some loads.
dnl  * Handle n = 4 early.
dnl  * Duplicate addmul loop into 4 loops which fall into each other.  Perhaps
dnl    stick to one mul_1 loop, but do the (mod 4) stuff at its end instead of
dnl    its beginning.

define(`rp', `x0')
define(`up', `x1')
define(`n',  `x2')

define(`v0', `x3')
define(`CY', `x17')

PROLOGUE(mpn_sqr_basecase)
	cmp	n, #3
	b.ls	L(le3)

	ldr	v0, [up],#8
	sub	n, n, #1
	mul	x6, v0, v0
	umulh	x4, v0, v0
	str	x6, [rp],#8
	lsl	v0, v0, 1
	lsl	n, n, #3
	lsr	x16, n, #5
	tbnz	n, #3, L(mbx1)

L(mbx0):adds	x11, x4, xzr		C move and clear cy
	tbz	n, #4, L(mb00)

L(mb10):ldp	x4, x5, [up],#16
	mul	x8, x4, v0
	umulh	x10, x4, v0
	cbz	x16, L(m2e)
	ldp	x6, x7, [up],#16
	mul	x9, x5, v0
	b	L(mmid)-8

L(mbx1):ldr	x7, [up],#8
	mul	x9, x7, v0
	umulh	x11, x7, v0
	adds	x9, x9, x4
	str	x9, [rp],#8
	tbnz	n, #4, L(mb10)
L(mb00):ldp	x6, x7, [up],#16
	mul	x8, x6, v0
	umulh	x10, x6, v0
	ldp	x4, x5, [up],#16
	mul	x9, x7, v0
	adcs	x12, x8, x11
	umulh	x11, x7, v0
	sub	x16, x16, #1
	cbz	x16, L(mend)

	ALIGN(16)
L(mtop):mul	x8, x4, v0
	ldp	x6, x7, [up],#16
	adcs	x13, x9, x10
	umulh	x10, x4, v0
	mul	x9, x5, v0
	stp	x12, x13, [rp],#16
	adcs	x12, x8, x11
	umulh	x11, x5, v0
L(mmid):mul	x8, x6, v0
	ldp	x4, x5, [up],#16
	adcs	x13, x9, x10
	umulh	x10, x6, v0
	mul	x9, x7, v0
	stp	x12, x13, [rp],#16
	adcs	x12, x8, x11
	umulh	x11, x7, v0
	sub	x16, x16, #1
	cbnz	x16, L(mtop)

L(mend):mul	x8, x4, v0
	adcs	x13, x9, x10
	umulh	x10, x4, v0
	stp	x12, x13, [rp],#16
L(m2e):	mul	x9, x5, v0
	adcs	x12, x8, x11
	umulh	x11, x5, v0
	adcs	x13, x9, x10
	stp	x12, x13, [rp],#16
	adc	x11, x11, xzr
	str	x11, [rp],#8

L(outer):
	sub	n, n, #8
	sub	rp, rp, n
	sub	up, up, n
	ldp	x6, x7, [up,#-16]
	ldr	v0, [rp,#-8]
	and	x8, x7, x6, asr 63
	mul	x9, x7, x7
	adds	v0, v0, x8
	umulh	x4, x7, x7
	adc	x4, x4, xzr
	adds	v0, v0, x9
	str	v0, [rp,#-8]
	adc	CY, x4, xzr
	adds	xzr, x6, x6
	adc	v0, x7, x7
	cmp	n, #16
	beq	L(cor2)

	lsr	x16, n, #5
	tbz	n, #3, L(bx0)

L(bx1):	ldr	x4, [up],#8
	mul	x8, x4, v0
	umulh	x4, x4, v0
	tbz	n, #4, L(b01)

L(b11):	ldp	x5, x6, [up],#16
	ldp	x12, x13, [rp]
	ldr	x14, [rp,#16]
	mul	x9, x5, v0
	umulh	x5, x5, v0
	mul	x10, x6, v0
	umulh	x6, x6, v0
	adds	x8, x12, x8
	adcs	x4, x13, x4
	adcs	x5, x14, x5
	adc	x6, x6, xzr
	adds	x8, x8, CY
	adcs	x4, x4, x9
	adcs	x5, x5, x10
	adc	CY, x6, xzr
	stp	x8, x4, [rp],#16
	str	x5, [rp],#8
	cbnz	x16, L(top)
	b	L(end)

L(b01):	ldr	x12, [rp]
	adds	x8, x12, x8
	adc	x4, x4, xzr
	adds	x8, x8, CY
	adc	CY, x4, xzr
	str	x8, [rp],#8
	b	L(top)

L(bx0):	ldp	x4, x5, [up],#16
	tbz	n, #4, L(top)+4

L(b10):	ldp	x12, x13, [rp]
	mul	x8, x4, v0
	umulh	x4, x4, v0
	mul	x9, x5, v0
	umulh	x5, x5, v0
	adds	x8, x12, x8
	adcs	x4, x13, x4
	adc	x5, x5, xzr
	adds	x8, x8, CY
	adcs	x4, x4, x9
	adc	CY, x5, xzr
	stp	x8, x4, [rp],#16

	ALIGN(16)
L(top):	ldp	x4, x5, [up],#16
	ldp	x6, x7, [up],#16
	ldp	x12, x13, [rp]
	ldp	x14, x15, [rp,#16]
	mul	x8, x4, v0
	umulh	x4, x4, v0
	mul	x9, x5, v0
	umulh	x5, x5, v0
	mul	x10, x6, v0
	umulh	x6, x6, v0
	mul	x11, x7, v0
	umulh	x7, x7, v0
	adds	x8, x12, x8
	adcs	x4, x13, x4
	adcs	x5, x14, x5
	adcs	x6, x15, x6
	adc	x7, x7, xzr
	adds	x8, x8, CY
	adcs	x4, x4, x9
	adcs	x5, x5, x10
	adcs	x6, x6, x11
	adc	CY, x7, xzr
	stp	x8, x4, [rp],#16
	stp	x5, x6, [rp],#16
	sub	x16, x16, #1
	cbnz	x16, L(top)

L(end):	str	CY, [rp],#8
	b	L(outer)

L(cor2):ldp	x10, x11, [up]
	ldp	x12, x13, [rp]
	mul	x8, x10, v0
	umulh	x4, x10, v0
	mul	x9, x11, v0
	umulh	x5, x11, v0
	adds	x8, x12, x8
	adcs	x4, x13, x4
	adc	x5, x5, xzr
	adds	x8, x8, CY
	adcs	x13, x4, x9
	adc	x12, x5, xzr
	str	x8, [rp]
	and	x8, x10, x7, asr 63
	mul	x9, x10, x10
	adds	x13, x13, x8
	umulh	x4, x10, x10
	adc	x4, x4, xzr
	adds	x13, x13, x9
	adc	CY, x4, xzr
	adds	xzr, x7, x7
	adc	v0, x10, x10
	mul	x8, x11, v0
	umulh	x4, x11, v0
	adds	x8, x12, x8
	adc	x4, x4, xzr
	adds	x8, x8, CY
	adc	v0, x4, xzr
	stp	x13, x8, [rp,#8]
	and	x2, x11, x10, asr 63
	mul	x5, x11, x11
	adds	v0, v0, x2
	umulh	x4, x11, x11
	adc	x4, x4, xzr
	adds	v0, v0, x5
	adc	x4, x4, xzr
	stp	v0, x4, [rp,#24]
	ret

L(le3):	ldr	v0, [up]
	mul	x4, v0, v0	C W0
	umulh	x5, v0, v0	C W1
	cmp	n, #2
	b.hs	L(2o3)
	stp	x4, x5, [rp]
	ret

L(2o3):	ldr	x6, [up,#8]
	mul	x7, x6, x6	C W2
	umulh	x8, x6, x6	C W3
	mul	x9, v0, x6	C W1+1/64
	umulh	x10, v0, x6	C W2+1/64
	b.hi	L(3)
	adds	x5, x5, x9	C W1
	adcs	x7, x7, x10	C W2
	adc	x8, x8, xzr	C W3
	adds	x5, x5, x9	C W1
	adcs	x7, x7, x10	C W2
	adc	x8, x8, xzr	C W3
	stp	x4, x5, [rp]
	stp	x7, x8, [rp,#16]
	ret

L(3):	ldr	x11, [up,#16]
	mul	x12, x11, x11	C W4
	umulh	x13, x11, x11	C W5
	mul	x14, v0, x11	C W2+1/64
	umulh	x15, v0, x11	C W3+1/64
	mul	x16, x6, x11	C W3+1/64
	umulh	x17, x6, x11	C W4+1/64
	adds	x5, x5, x9
	adcs	x7, x7, x10
	adcs	x8, x8, x15
	adcs	x12, x12, x17
	adc	x13, x13, xzr
	adds	x5, x5, x9
	adcs	x7, x7, x10
	adcs	x8, x8, x15
	adcs	x12, x12, x17
	adc	x13, x13, xzr
	adds	x7, x7, x14
	adcs	x8, x8, x16
	adcs	x12, x12, xzr
	adc	x13, x13, xzr
	adds	x7, x7, x14
	adcs	x8, x8, x16
	adcs	x12, x12, xzr
	adc	x13, x13, xzr
	stp	x4, x5, [rp]
	stp	x7, x8, [rp,#16]
	stp	x12, x13, [rp,#32]
	ret
EPILOGUE()
