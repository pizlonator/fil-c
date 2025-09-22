dnl  SPARC v8 mpn_addmul_1 -- Multiply a limb vector with a limb and
dnl  add the result to a second limb vector.

dnl  Copyright 1992-1995, 2000 Free Software Foundation, Inc.

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
C res_ptr	o0
C s1_ptr	o1
C size		o2
C s2_limb	o3

ASM_START()
PROLOGUE(mpn_addmul_1)
	ld	[%o1+0],%o4
	andcc	%o2,1,%g0
	be	L(bx0)
	andcc	%o2,2,%g0
L(bx1):	be	L(01)
	orcc	%g0,%g0,%g2
L(b11):	add	%o0,-8,%o0
	b	L(11)
	add	%o1,-8,%o1
L(bx0):	be	L(b00)
	orcc	%g0,%g0,%g2
L(b10):	add	%o0,-12,%o0
	b	L(10)
	add	%o1,4,%o1
L(b00):	add	%o0,-4,%o0
	b	L(00)
	add	%o1,-4,%o1

L(top):	addcc	%g3,%g2,%g3	C 1
	ld	[%o1+4],%o4	C 2
	rd	%y,%g2		C 1
	addx	%g0,%g2,%g2
	ld	[%o0+0],%g1	C 2
	addcc	%g1,%g3,%g3
	st	%g3,[%o0+0]	C 1
L(00):	umul	%o4,%o3,%g3	C 2
	ld	[%o0+4],%g1	C 2
	addxcc	%g3,%g2,%g3	C 2
	ld	[%o1+8],%o4	C 3
	rd	%y,%g2		C 2
	addx	%g0,%g2,%g2
	nop
	addcc	%g1,%g3,%g3
	st	%g3,[%o0+4]	C 2
L(11):	umul	%o4,%o3,%g3	C 3
	addxcc	%g3,%g2,%g3	C 3
	ld	[%o1+12],%o4	C 4
	rd	%y,%g2		C 3
	add	%o1,16,%o1
	addx	%g0,%g2,%g2
	ld	[%o0+8],%g1	C 2
	addcc	%g1,%g3,%g3
	st	%g3,[%o0+8]	C 3
L(10):	umul	%o4,%o3,%g3	C 4
	addxcc	%g3,%g2,%g3	C 4
	ld	[%o1+0],%o4	C 1
	rd	%y,%g2		C 4
	addx	%g0,%g2,%g2
	ld	[%o0+12],%g1	C 2
	addcc	%g1,%g3,%g3
	st	%g3,[%o0+12]	C 4
	add	%o0,16,%o0
	addx	%g0,%g2,%g2
L(01):	addcc	%o2,-4,%o2
	bg	L(top)
	umul	%o4,%o3,%g3	C 1

	addcc	%g3,%g2,%g3	C 4
	rd	%y,%g2		C 4
	addx	%g0,%g2,%g2
	ld	[%o0+0],%g1	C 2
	addcc	%g1,%g3,%g3
	st	%g3,[%o0+0]	C 4

	retl
	addx	%g0,%g2,%o0
EPILOGUE(mpn_addmul_1)
