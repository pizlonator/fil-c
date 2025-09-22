dnl  ARM64 mpn_addaddmul_1msb0, R = Au + Bv, u,v < 2^63.

dnl  Copyright 2021 Free Software Foundation, Inc.

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
C Apple M1	 2.0

changecom(blah)

define(`rp', x0)
define(`ap', x1)
define(`bp', x2)
define(`n',  x3)
define(`u0', x4)
define(`v0', x5)

C TODO
C * Use fewer distinct registers, should be trivial.

PROLOGUE(mpn_addaddmul_1msb0)
	lsr	x7, n, #1
	adds	x6, xzr, xzr
	tbz	n, #0, L(top)

	ldr	x11, [ap], #8		C 0
	ldr	x15, [bp], #8		C 0
	mul	x10, x11, u0		C 0
	umulh	x11, x11, u0		C 1
	mul	x14, x15, v0		C 0
	umulh	x15, x15, v0		C 1
	adds	x10, x10, x14		C 0
	adcs	x6, x11, x15		C 1
	str	x10, [rp], #8		C 0
	cbz	x7, L(end)

L(top):	ldp	x11, x13, [ap], #16	C 0 1
	ldp	x15, x17, [bp], #16	C 0 1
	mul	x10, x11, u0		C 0
	umulh	x11, x11, u0		C 1
	mul	x14, x15, v0		C 0
	umulh	x15, x15, v0		C 1
	adcs	x10, x10, x14		C 0
	adc	x11, x11, x15		C 1
	adds	x10, x10, x6		C 0
	mul	x12, x13, u0		C 1
	umulh	x13, x13, u0		C 2
	mul	x14, x17, v0		C 1
	umulh	x17, x17, v0		C 2
	adcs	x12, x12, x14		C 1
	adc	x6, x13, x17		C 2
	adds	x11, x12, x11		C 1
	stp	x10, x11, [rp], #16	C 0 1
	sub	x7, x7, #1
	cbnz	x7, L(top)

L(end):	adc	x0, x6, xzr
	ret
EPILOGUE()
