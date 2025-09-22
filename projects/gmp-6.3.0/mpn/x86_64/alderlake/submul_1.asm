dnl  AMD64 mpn_submul_1 for CPUs with mulx and adx.

dnl  Contributed to the GNU project by Torbjörn Granlund.

dnl  Copyright 2022 Free Software Foundation, Inc.

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
C AMD K8,K9	 -
C AMD K10	 -
C AMD bd1	 -
C AMD bd2	 -
C AMD bd3	 -
C AMD bd4	 -
C AMD zn1	 ?
C AMD zn2	 ?
C AMD zn3	 2.0
C AMD bt1	 -
C AMD bt2	 -
C Intel P4	 -
C Intel CNR	 -
C Intel PNR	 -
C Intel NHM	 -
C Intel WSM	 -
C Intel SBR	 -
C Intel IBR	 -
C Intel HWL	 -
C Intel BWL	 ?
C Intel SKL	 ?
C Intel RKL	 2.0
C Intel ALD	 1.53
C Intel atom	 -
C Intel SLM	 -
C Intel GLM	 -
C VIA nano	 -

define(`rp',      `%rdi')	dnl rcx
define(`up',      `%rsi')	dnl rdx
define(`n_param', `%rdx')	dnl r8
define(`v0_param',`%rcx')	dnl r9

define(`n',       `%rcx')	dnl
define(`v0',      `%rdx')	dnl


ASM_START()
	TEXT
	ALIGN(16)
PROLOGUE(mpn_submul_1)
	mov	n_param, %rax
	mov	v0_param, v0
	mov	%rax, n
	test	$1, R8(n)
	jz	L(bx0)

L(bx1):	mulx(	(up), %r9, %rax)
	test	$2, R8(n)
	stc
	jz	L(b01)

L(b11):	lea	1(n), n
	lea	16(up), up
	lea	16(rp), rp
	jmp	L(lo3)

L(b01):	lea	3(n), n
	jmp	L(lo1)

L(bx0):	mulx(	(up), %r9, %r8)
	test	$2, R8(n)
	stc
	jz	L(b00)

L(b10):	lea	8(up), up
	lea	8(rp), rp
	lea	2(n), n
	jmp	L(lo2)

L(b00):	lea	24(up), up
	lea	24(rp), rp
	jmp	L(lo0)

L(top):	lea	32(up), up
	lea	32(rp), rp
	mulx(	-24,(up), %r9, %r8)
	adox(	%rax, %r9)
L(lo0):	not	%r9
	adcx(	-24,(rp), %r9)
	mov	%r9, -24(rp)
	mulx(	-16,(up), %r9, %rax)
	adox(	%r8, %r9)
L(lo3):	not	%r9
	adcx(	-16,(rp), %r9)
	mov	%r9, -16(rp)
	mulx(	-8,(up), %r9, %r8)
	adox(	%rax, %r9)
L(lo2):	not	%r9
	adcx(	-8,(rp), %r9)
	mov	%r9, -8(rp)
	mulx(	(up), %r9, %rax)
	adox(	%r8, %r9)
L(lo1):	not	%r9
	adcx(	(rp), %r9)
	mov	%r9, (rp)
	lea	-4(n), n
	jrcxz	L(end)
	jmp	L(top)

L(end):	adox(	%rcx, %rax)
	sbb	$-1, %rax
	ret
EPILOGUE()
ASM_END()
