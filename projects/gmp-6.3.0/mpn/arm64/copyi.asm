dnl  ARM64 mpn_copyi.

dnl  Copyright 2013, 2020 Free Software Foundation, Inc.

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
C Cortex-A53	 1.8
C Cortex-A55	 1.28
C Cortex-A57
C Cortex-A72	 1
C Cortex-A73	 1.1-1.35 (alignment dependent)
C X-Gene	 1
C Apple M1	 0.31

changecom(blah)

define(`rp', `x0')
define(`up', `x1')
define(`n',  `x2')

ASM_START()
PROLOGUE(mpn_copyi)
	cmp	n, #3
	b.le	L(bc)

C Copy until rp is 128-bit aligned
	tbz	rp, #3, L(al2)
	ldr	x4, [up],#8
	sub	n, n, #1
	str	x4, [rp],#8

L(al2):	ldp	x4,x5, [up],#16
	sub	n, n, #6
	tbnz	n, #63, L(end)

	ALIGN(16)
L(top):	ldp	x6,x7, [up],#32
	stp	x4,x5, [rp],#32
	ldp	x4,x5, [up,#-16]
	stp	x6,x7, [rp,#-16]
	sub	n, n, #4
	tbz	n, #63, L(top)

L(end):	stp	x4,x5, [rp],#16

C Copy last 0-3 limbs.  Note that rp is aligned after loop, but not when we
C arrive here via L(bc)
L(bc):	tbz	n, #1, L(tl1)
	ldp	x4,x5, [up],#16
	stp	x4,x5, [rp],#16
L(tl1):	tbz	n, #0, L(tl2)
	ldr	x4, [up]
	str	x4, [rp]
L(tl2):	ret
EPILOGUE()
