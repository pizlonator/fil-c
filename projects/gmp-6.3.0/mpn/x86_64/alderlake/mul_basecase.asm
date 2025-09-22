dnl  AMD64 mpn_mul_basecase.

dnl  Contributed to the GNU project by Torbjörn Granlund.

dnl  Copyright 2012, 2013, 2022 Free Software Foundation, Inc.

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
C AMD zn3	 ?
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
C Intel RKL	 ?
C Intel ALD	 1.29
C Intel atom	 -
C Intel SLM	 -
C Intel GLM	 -
C VIA nano	 -

C TODO
C  * Do overlapped software pipelining.
C  * Try shallower pipeline, which would result in using fewer registers.
C  * There are false dependencies on CF/OF between iterations.  Try breaking
C    them to see if it helps.

define(`rp',	`%rdi')	dnl rcx
define(`up',	`%rsi')	dnl rdx
define(`un_arg',`%rdx')	dnl r8
define(`vp_arg',`%rcx')	dnl r9
define(`vn_arg',`%r8')	dnl stack

define(`un',	`%r14')
define(`vp',	`%r15')
define(`vn',	`%rbp')

define(`n',	`%rcx')
define(`v0',	`%rdx')


ASM_START()
	TEXT
	ALIGN(16)
PROLOGUE(mpn_mul_basecase)
	cmp	$2, un_arg
	ja	L(gen)
	mov	(vp_arg), %rdx
	mulx(	(up), %rax, %r9)
	mov	%rax, (rp)
	je	L(s2x)

	mov	%r9, 8(rp)
	ret

L(s2x):	mulx(	8,(up), %rax, %r10)
	add	%r9, %rax
	adc	$0, %r10
	cmp	$2, R32(vn_arg)
	je	L(s22)

L(s21):	mov	%rax, 8(rp)
	mov	%r10, 16(rp)
	ret

L(s22):	mov	8(vp_arg), %rdx
	mulx(	(up), %r8, %r9)
	add	%r8, %rax
	adc	%r10, %r9
	mov	%rax, 8(rp)
	mulx(	8,(up), %rax, %r10)
	adc	$0, %r10
	adc	%r9, %rax
	mov	%rax, 16(rp)
	adc	$0, %r10
	mov	%r10, 24(rp)
	ret

L(gen):	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	mov	un_arg, un
	neg	un
	shl	$3, un
	mov	vp_arg, vp
	mov	vn_arg, vn

	test	$1, R8(un_arg)
	mov	(vp), %rdx
	jz	L(bx0)

L(bx1):	test	$16, R8(un)
	jnz	L(b01)

L(b11):	lea	24(un), n
	mulx(	(up), %r11, %r10)
	mulx(	8,(up), %r13, %r12)
	mulx(	16,(up), %rbx, %rax)
	lea	8(rp), rp
	lea	24(up), up
	jrcxz	L(med3)
L(mtp3):mulx(	(up), %r9, %r8)
	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	mulx(	8,(up), %r11, %r10)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	mulx(	16,(up), %r13, %r12)
	adcx(	%rax, %r9)
	mov	%rbx, 8(rp)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, 16(rp)
	lea	32(up), up
	lea	32(rp), rp
	lea	32(n), n
	jrcxz	L(med3)
	jmp	L(mtp3)
L(med3):adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adcx(	%rcx, %rax)
	mov	%rbx, 8(rp)
	mov	%rax, 16(rp)
	dec	vn
	jz	L(ret)
L(out3):lea	32(rp,un), rp
	lea	24(up,un), up
	lea	8(vp), vp
	xor	R32(%rdx), R32(%rdx)
	mov	(vp), %rdx
	mulx(	-24,(up), %r11, %r10)
	mulx(	-16,(up), %r13, %r12)
	mulx(	-8,(up), %rbx, %rax)
	lea	24(un), n
	adox(	-8,(rp), %r11)
	jrcxz	L(ed3)
L(tp3):	mulx(	(up), %r9, %r8)
	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adox(	(rp), %r13)
	mulx(	8,(up), %r11, %r10)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adox(	8,(rp), %rbx)
	mulx(	16,(up), %r13, %r12)
	adcx(	%rax, %r9)
	mov	%rbx, 8(rp)
	adox(	16,(rp), %r9)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, 16(rp)
	adox(	24,(rp), %r11)
	lea	32(up), up
	lea	32(rp), rp
	lea	32(n), n
	jrcxz	L(ed3)
	jmp	L(tp3)
L(ed3):	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adox(	(rp), %r13)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adox(	8,(rp), %rbx)
	adcx(	%rcx, %rax)
	adox(	%rcx, %rax)
	mov	%rbx, 8(rp)
	mov	%rax, 16(rp)
	dec	vn
	jnz	L(out3)
	jmp	L(ret)


L(b01):	mulx(	(up), %rbx, %rax)
	lea	8(un), n
	mulx(	8,(up), %r9, %r8)
	mulx(	16,(up), %r11, %r10)
	lea	8(up), up
	lea	-8(rp), rp
	jmp	L(ml1)
L(mtp1):mulx(	(up), %r9, %r8)
	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	mulx(	8,(up), %r11, %r10)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
L(ml1):	mulx(	16,(up), %r13, %r12)
	adcx(	%rax, %r9)
	mov	%rbx, 8(rp)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, 16(rp)
	lea	32(up), up
	lea	32(rp), rp
	lea	32(n), n
	jrcxz	L(med1)
	jmp	L(mtp1)
L(med1):adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adcx(	%rcx, %rax)
	mov	%rbx, 8(rp)
	mov	%rax, 16(rp)
	dec	vn
	jz	L(ret)
L(out1):lea	16(rp,un), rp
	lea	8(up,un), up
	lea	8(vp), vp
	xor	R32(%rdx), R32(%rdx)
	mov	(vp), %rdx
	lea	8(un), n
	mulx(	-8,(up), %rbx, %rax)
	mulx(	(up), %r9, %r8)
	mulx(	8,(up), %r11, %r10)
	jmp	L(lo1)
L(tp1):	mulx(	(up), %r9, %r8)
	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adox(	(rp), %r13)
	mulx(	8,(up), %r11, %r10)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
L(lo1):	adox(	8,(rp), %rbx)
	mulx(	16,(up), %r13, %r12)
	adcx(	%rax, %r9)
	mov	%rbx, 8(rp)
	adox(	16,(rp), %r9)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, 16(rp)
	adox(	24,(rp), %r11)
	lea	32(up), up
	lea	32(rp), rp
	lea	32(n), n
	jrcxz	L(ed1)
	jmp	L(tp1)
L(ed1):	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adox(	(rp), %r13)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adox(	8,(rp), %rbx)
	adcx(	%rcx, %rax)
	adox(	%rcx, %rax)
	mov	%rbx, 8(rp)
	mov	%rax, 16(rp)
	dec	vn
	jnz	L(out1)
	jmp	L(ret)


L(bx0):	test	$16, R8(un)
	jz	L(b00)

L(b10):	mulx(	(up), %r13, %r12)
	mulx(	8,(up), %rbx, %rax)
	lea	16(un), n
	mulx(	16,(up), %r9, %r8)
	lea	16(up), up
	jmp	L(ml2)
L(mtp2):mulx(	(up), %r9, %r8)
	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
L(ml2):	mulx(	8,(up), %r11, %r10)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	mulx(	16,(up), %r13, %r12)
	adcx(	%rax, %r9)
	mov	%rbx, 8(rp)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, 16(rp)
	lea	32(up), up
	lea	32(rp), rp
	lea	32(n), n
	jrcxz	L(med2)
	jmp	L(mtp2)
L(med2):adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adcx(	%rcx, %rax)
	mov	%rbx, 8(rp)
	mov	%rax, 16(rp)
	dec	vn
	jz	L(ret)
L(out2):lea	24(rp,un), rp
	lea	16(up,un), up
	lea	8(vp), vp
	xor	R32(%rdx), R32(%rdx)
	mov	(vp), %rdx
	mulx(	-16,(up), %r13, %r12)
	mulx(	-8,(up), %rbx, %rax)
	lea	16(un), n
	mulx(	(up), %r9, %r8)
	jmp	L(lo2)
L(tp2):	mulx(	(up), %r9, %r8)
	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
L(lo2):	adox(	(rp), %r13)
	mulx(	8,(up), %r11, %r10)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adox(	8,(rp), %rbx)
	mulx(	16,(up), %r13, %r12)
	adcx(	%rax, %r9)
	mov	%rbx, 8(rp)
	adox(	16,(rp), %r9)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, 16(rp)
	adox(	24,(rp), %r11)
	lea	32(up), up
	lea	32(rp), rp
	lea	32(n), n
	jrcxz	L(ed2)
	jmp	L(tp2)
L(ed2):	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adox(	(rp), %r13)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adox(	8,(rp), %rbx)
	adcx(	%rcx, %rax)
	adox(	%rcx, %rax)
	mov	%rbx, 8(rp)
	mov	%rax, 16(rp)
	dec	vn
	jnz	L(out2)
	jmp	L(ret)


L(b00):	lea	32(un), n
	mulx(	(up), %r9, %r8)
	mulx(	8,(up), %r11, %r10)
	mulx(	16,(up), %r13, %r12)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, (rp)
	lea	32(up), up
	lea	16(rp), rp
	jrcxz	L(med0)
L(mtp0):mulx(	(up), %r9, %r8)
	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	mulx(	8,(up), %r11, %r10)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	mulx(	16,(up), %r13, %r12)
	adcx(	%rax, %r9)
	mov	%rbx, 8(rp)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, 16(rp)
	lea	32(up), up
	lea	32(rp), rp
	lea	32(n), n
	jrcxz	L(med0)
	jmp	L(mtp0)
L(med0):adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adcx(	%rcx, %rax)
	mov	%rbx, 8(rp)
	mov	%rax, 16(rp)
	dec	vn
	jz	L(ret)
L(out0):lea	40(rp,un), rp
	lea	32(up,un), up
	lea	8(vp), vp
	xor	R32(%rdx), R32(%rdx)
	mov	(vp), %rdx
	lea	32(un), n
	mulx(	-32,(up), %r9, %r8)
	mulx(	-24,(up), %r11, %r10)
	mulx(	-16,(up), %r13, %r12)
	adox(	-16,(rp), %r9)
	mulx(	-8,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, -16(rp)
	adox(	-8,(rp), %r11)
	jrcxz	L(ed0)
L(tp0):	mulx(	(up), %r9, %r8)
	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adox(	(rp), %r13)
	mulx(	8,(up), %r11, %r10)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adox(	8,(rp), %rbx)
	mulx(	16,(up), %r13, %r12)
	adcx(	%rax, %r9)
	mov	%rbx, 8(rp)
	adox(	16,(rp), %r9)
	mulx(	24,(up), %rbx, %rax)
	adcx(	%r8, %r11)
	mov	%r9, 16(rp)
	adox(	24,(rp), %r11)
	lea	32(up), up
	lea	32(rp), rp
	lea	32(n), n
	jrcxz	L(ed0)
	jmp	L(tp0)
L(ed0):	adcx(	%r10, %r13)
	mov	%r11, -8(rp)
	adox(	(rp), %r13)
	adcx(	%r12, %rbx)
	mov	%r13, (rp)
	adox(	8,(rp), %rbx)
	adcx(	%rcx, %rax)
	adox(	%rcx, %rax)
	mov	%rbx, 8(rp)
	mov	%rax, 16(rp)
	dec	vn
	jnz	L(out0)

L(ret):	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	ret
EPILOGUE()
ASM_END()
