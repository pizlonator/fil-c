dnl  ARM64 mpn_addmul_1 and mpn_submul_1.

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

C	     cycles/limb
C Cortex-A53
C Cortex-A55
C Cortex-A57
C Cortex-A72
C Cortex-A73
C X-Gene
C Apple M1	 1.25

changecom(blah)

define(`rp', x0)
define(`up', x1)
define(`n',  x2)
define(`v0', x3)
define(`cin',x4)

define(`CY',x17)

ifdef(`OPERATION_addmul_1', `
  define(`ADDSUB',	adds)
  define(`ADDSUBC',	adcs)
  define(`COND',	`cc')
  define(`func',	mpn_addmul_1)')
ifdef(`OPERATION_submul_1', `
  define(`ADDSUB',	subs)
  define(`ADDSUBC',	sbcs)
  define(`COND',	`cs')
  define(`func',	mpn_submul_1)')

MULFUNC_PROLOGUE(mpn_addmul_1 mpn_submul_1 mpn_addmul_1c)

ifdef(`OPERATION_addmul_1', `
PROLOGUE(mpn_addmul_1c)
	mov	CY, cin
	b	L(ent)
EPILOGUE()
')

PROLOGUE(func)
	mov	CY, #0		C W0
L(ent):	lsr	x16, n, #2
	tbz	n, #0, L(bx0)

L(bx1):	ldr	x4, [up], #8
	mul	x8, x4, v0
	umulh	x4, x4, v0
	tbz	n, #1, L(b01)

L(b11):	ldp	x5,x6, [up], #16
	ldp	x12,x13, [rp]
	ldr	x14, [rp,#16]
	mul	x9, x5, v0
	umulh	x5, x5, v0
	mul	x10, x6, v0
	umulh	x6, x6, v0
	ADDSUB	x8, x12, x8
	ADDSUBC	x4, x13, x4
	ADDSUBC	x5, x14, x5
	csinc	x6, x6, x6, COND
	ADDSUB	x8, x8, CY
	ADDSUBC	x4, x4, x9
	ADDSUBC	x5, x5, x10
	csinc	CY, x6, x6, COND
	stp	x8, x4, [rp], #16
	str	x5, [rp], #8
	cbnz	x16, L(top)
	mov	x0, CY
	ret

L(b01):	ldr	x12, [rp]
	ADDSUB	x8, x12, x8
	csinc	x4, x4, x4, COND
	ADDSUB	x8, x8, CY
	csinc	CY, x4, x4, COND
	str	x8, [rp], #8
	cbnz	x16, L(top)
	mov	x0, CY
	ret

L(bx0):	ldp	x4,x5, [up], #16
	tbz	n, #1, L(top)+4

L(b10):	ldp	x12,x13, [rp]
	mul	x8, x4, v0
	umulh	x4, x4, v0
	mul	x9, x5, v0
	umulh	x5, x5, v0
	ADDSUB	x8, x12, x8
	ADDSUBC	x4, x13, x4
	csinc	x5, x5, x5, COND
	ADDSUB	x8, x8, CY
	ADDSUBC	x4, x4, x9
	csinc	CY, x5, x5, COND
	stp	x8, x4, [rp], #16
	cbz	x16, L(done)

L(top):	ldp	x4,x5, [up], #16	C W0 W1
	ldp	x6,x7, [up], #16	C W2 W3
	ldp	x12,x13, [rp]		C W0 W1
	ldp	x14,x15, [rp,#16]	C W2 W3
	mul	x8, x4, v0		C W0
	umulh	x4, x4, v0		C W1
	mul	x9, x5, v0		C W1
	umulh	x5, x5, v0		C W2
	mul	x10, x6, v0		C W2
	umulh	x6, x6, v0		C W3
	mul	x11, x7, v0		C W3
	umulh	x7, x7, v0		C W4
	ADDSUB	x8, x12, x8		C W0
	ADDSUBC	x4, x13, x4		C W1
	ADDSUBC	x5, x14, x5		C W2
	ADDSUBC	x6, x15, x6		C W3
	csinc	x7, x7, x7, COND	C W4
	ADDSUB	x8, x8, CY		C W0	carry-in
	ADDSUBC	x4, x4, x9		C W1
	ADDSUBC	x5, x5, x10		C W2
	ADDSUBC	x6, x6, x11		C W2
	csinc	CY, x7, x7, COND	C W3	carry-out
	stp	x8, x4, [rp], #16
	stp	x5, x6, [rp], #16
	sub	x16, x16, #1
	cbnz	x16, L(top)

L(done):mov	x0, CY
	ret
EPILOGUE()
