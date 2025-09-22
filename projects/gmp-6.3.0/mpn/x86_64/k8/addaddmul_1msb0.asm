dnl  AMD64 mpn_addaddmul_1msb0, R = Au + Bv, u,v < 2^63.

dnl  Copyright 2008, 2021 Free Software Foundation, Inc.

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
C AMD K8,K9	 2.167
C AMD K10	 2.167
C Intel P4	12.0
C Intel core2	 4.0
C Intel corei	 ?
C Intel atom	 ?
C VIA nano	 ?

C TODO
C  * Perhaps handle various n mod 3 sizes better.  The code now is too large.

C INPUT PARAMETERS
define(`rp',	`%rdi')
define(`ap',	`%rsi')
define(`bp_param', `%rdx')
define(`n',	`%rcx')
define(`u0',	`%r8')
define(`v0',	`%r9')


define(`bp', `%rbp')

ABI_SUPPORT(DOS64)
ABI_SUPPORT(STD64)

ASM_START()
	TEXT
	ALIGN(16)
PROLOGUE(mpn_addaddmul_1msb0)
        FUNC_ENTRY(4)
IFDOS(`	mov	56(%rsp), %r8	')
IFDOS(`	mov	64(%rsp), %r9	')
	push	%rbp

	lea	(ap,n,8), ap
	lea	(bp_param,n,8), bp
	lea	(rp,n,8), rp
	neg	n

	mov	(ap,n,8), %rax
	mul	%r8
	mov	%rax, %r11
	mov	(bp,n,8), %rax
	mov	%rdx, %r10
	add	$3, n
	jns	L(end)

	push	%r13

	ALIGN(16)
L(top):	mul	%r9
	add	%rax, %r11
	mov	-16(ap,n,8), %rax
	adc	%rdx, %r10
	mov	%r11, -24(rp,n,8)
	mul	%r8
	add	%rax, %r10
	mov	-16(bp,n,8), %rax
	mov	$0, R32(%r13)
	adc	%rdx, %r13
	mul	%r9
	add	%rax, %r10
	mov	-8(ap,n,8), %rax
	adc	%rdx, %r13
	mov	%r10, -16(rp,n,8)
	mul	%r8
	add	%rax, %r13
	mov	-8(bp,n,8), %rax
	mov	$0, R32(%r11)
	adc	%rdx, %r11
	mul	%r9
	add	%rax, %r13
	adc	%rdx, %r11
	mov	(ap,n,8), %rax
	mul	%r8
	add	%rax, %r11
	mov	%r13, -8(rp,n,8)
	mov	(bp,n,8), %rax
	mov	$0, R32(%r10)
	adc	%rdx, %r10
	add	$3, n
	js	L(top)

	pop	%r13

L(end):	mul	%r9
	add	%rax, %r11
	adc	%rdx, %r10
	cmp	$1, R32(n)
	ja	L(two)
	mov	-16(ap,n,8), %rax
	mov	%r11, -24(rp,n,8)
	mov	%r10, %r11
	jz	L(one)

L(nul):	mul	%r8
	add	%rax, %r10
	mov	-16(bp), %rax
	mov	$0, R32(%r11)
	adc	%rdx, %r11
	mul	%r9
	add	%rax, %r10
	mov	-8(ap), %rax
	adc	%rdx, %r11
	mov	%r10, -16(rp)
L(one):	mul	%r8
	add	%rax, %r11
	mov	-8(bp), %rax
	mov	$0, R32(%r10)
	adc	%rdx, %r10
	mul	%r9
	add	%rax, %r11
	adc	%rdx, %r10

L(two):	mov	%r11, -8(rp)
	mov	%r10, %rax
L(ret):	pop	%rbp
	FUNC_EXIT()
	ret
EPILOGUE()
