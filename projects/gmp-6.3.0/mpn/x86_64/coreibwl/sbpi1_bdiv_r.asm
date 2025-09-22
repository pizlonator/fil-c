dnl  AMD64 mpn_sbpi1_bdiv_r optimised for Intel Broadwell.

dnl  Copyright 2015, 2021 Free Software Foundation, Inc.

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

C cycles/limb	mul_1		addmul_1
C AMD K8,K9	n/a		n/a
C AMD K10	n/a		n/a
C AMD bd1	n/a		n/a
C AMD bd2	n/a		n/a
C AMD bd3	n/a		n/a
C AMD bd4	 ?		 ?
C AMD zn1	 ?		 ?
C AMD zn2	 ?		 ?
C AMD zn3	 ?		 ?
C AMD bt1	n/a		n/a
C AMD bt2	n/a		n/a
C Intel P4	n/a		n/a
C Intel PNR	n/a		n/a
C Intel NHM	n/a		n/a
C Intel SBR	n/a		n/a
C Intel IBR	n/a		n/a
C Intel HWL	 1.68		n/a
C Intel BWL	 1.51	      1.67-1.74
C Intel SKL	 1.52	      1.63-1.71
C Intel atom	n/a		n/a
C Intel SLM	n/a		n/a
C VIA nano	n/a		n/a

C The inner loops of this code are the result of running a code generation and
C optimisation tool suite written by David Harvey and Torbjorn Granlund.

C TODO
C  * Do overlapped software pipelining.
C  * Reduce register use, i.e., by combining n_neg and n_save.
C  * Supporess initial store through up, it's always a zero.
C  * Streamline up and dp setup.
C  * When changing this, make sure the code which falls into the inner loops
C    does not execute too many no-ops (for both PIC and non-PIC).

dnl  mp_limb_t
dnl  mpn_sbpi1_bdiv_r (mp_ptr up, mp_size_t un,
dnl		       mp_srcptr dp, mp_size_t dn, mp_limb_t dinv)

define(`up',      `%rdi')
define(`un',      `%rsi')
define(`dp_param',`%rdx')
define(`dn_param',`%rcx')
define(`dinv',    `%r8')

define(`n',       `%rcx')
define(`n_save',  `%rbp')
define(`dp',      `%r14')
define(`n_neg',   `%rbx')
define(`q',       `%rdx')
define(`jaddr',   `%rax')

define(`w0',	`%r12')
define(`w1',	`%r9')
define(`w2',	`%r10')
define(`w3',	`%r11')

ifdef(`MAX_SPECIAL',,`
define(`MAX_SPECIAL', 8)')

ABI_SUPPORT(DOS64)
ABI_SUPPORT(STD64)

ASM_START()
	TEXT
	ALIGN(16)
PROLOGUE(mpn_sbpi1_bdiv_r)
	FUNC_ENTRY(4)
IFDOS(`	mov	56(%rsp), %r8	')

	lea	L(atab)(%rip), %r10

	cmp	$MAX_SPECIAL, dn_param
	jbe	L(sma)

ifelse(MAX_SPECIAL,8,,`
forloop(i,eval(MAX_SPECIAL+1),9,`L(i):
')')

L(gen):	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14

	mov	dp_param, dp		C free up rdx
	xor	%r13, %r13

	sub	dn_param, un		C outer loop count

	lea	-8(,dn_param,8), n_neg
	neg	n_neg
	mov	dn_param, n_save
	mov	R32(dn_param), R32(%rax)
	shr	$3, n_save		C loop count
	and	$7, R32(%rax)		C clear CF and OF as side-effect

ifdef(`PIC',
`	movslq	(%r10,%rax,4), %rax
	lea	(%rax,%r10), jaddr
',`
	mov	(%r10,%rax,8), jaddr
')
	mov	(up), q
	imul	dinv, q
	jmp	L(outer)

L(f0):	mulx(	(dp), w2, w3)
	lea	-1(n), n
	mulx(	8,(dp), w0, w1)
	lea	-8(dp), dp
	adcx(	w3, w0)
	adox(	(up), w2)
	lea	-8(up), up
	jmp	L(b0x)

L(f3):	mulx(	(dp), w0, w1)
	mulx(	8,(dp), w2, w3)
	adox(	(up), w0)
	lea	-48(up), up
	lea	16(dp), dp
	jmp	L(b3x)

L(f4):	mulx(	(dp), w2, w3)
	mulx(	8,(dp), w0, w1)
	lea	24(dp), dp
	adox(	(up), w2)
	lea	-40(up), up
	adcx(	w3, w0)
	jmp	L(b4x)

L(f5):	mulx(	(dp), w0, w1)
	mulx(	8,(dp), w2, w3)
	lea	32(dp), dp
	adcx(	w1, w2)
	adox(	(up), w0)
	lea	-32(up), up
	jmp	L(b5x)

L(f6):	mulx(	(dp), w2, w3)
	mulx(	8,(dp), w0, w1)
	lea	40(dp), dp
	adox(	(up), w2)
	lea	-24(up), up
	adcx(	w3, w0)
	jmp	L(b6x)

L(f7):	mulx(	(dp), w0, w1)
	mulx(	8,(dp), w2, w3)
	lea	48(dp), dp
	adcx(	w1, w2)
	adox(	(up), w0)
	lea	-16(up), up
	jmp	L(b7x)

L(f1):	mulx(	(dp), w0, w1)
	mulx(	8,(dp), w2, w3)
	adox(	(up), w0)
	lea	-1(n), n
	jmp	L(b1x)

L(f2):	mulx(	(dp), w2, w3)
	mulx(	8,(dp), w0, w1)
	lea	8(dp), dp
	adox(	(up), w2)
	lea	8(up), up
	adcx(	w3, w0)
	jmp	L(b2x)

L(end):	adox(	(up), w0)
	adox(	%rcx, w1)		C relies on rcx = 0
	mov	w0, (up)
	adc	%rcx, w1		C relies on rcx = 0
	mov	8(up,n_neg), q		C Compute next quotient early...
	mulx(	dinv, q, %r12)		C ...(unused in last iteration)
	bt	$0, R32(%r13)
	adc	w1, 8(up)
	setc	R8(%r13)
	dec	un			C clear OF as side-effect
	jz	L(done)

	lea	(dp,n_neg), dp		C reset dp to D[]'s beginning
	lea	8(up,n_neg), up		C point up to U[]'s current beginning
L(outer):
	mov	n_save, n
	test	%eax, %eax		C clear CF and OF
	jmp	*jaddr

	ALIGN(16)
L(top):	adox(	-8,(up), w2)
	adcx(	w3, w0)
	mov	w2, -8(up)
	jrcxz	L(end)
L(b2x):	mulx(	8,(dp), w2, w3)
	adox(	(up), w0)
	lea	-1(n), n
	mov	w0, (up)
L(b1x):	adcx(	w1, w2)
	mulx(	16,(dp), w0, w1)
	adcx(	w3, w0)
	adox(	8,(up), w2)
	mov	w2, 8(up)
L(b0x):	mulx(	24,(dp), w2, w3)
	lea	64(dp), dp
	adcx(	w1, w2)
	adox(	16,(up), w0)
	mov	w0, 16(up)
L(b7x):	mulx(	-32,(dp), w0, w1)
	adox(	24,(up), w2)
	adcx(	w3, w0)
	mov	w2, 24(up)
L(b6x):	mulx(	-24,(dp), w2, w3)
	adcx(	w1, w2)
	adox(	32,(up), w0)
	mov	w0, 32(up)
L(b5x):	mulx(	-16,(dp), w0, w1)
	adox(	40,(up), w2)
	adcx(	w3, w0)
	mov	w2, 40(up)
L(b4x):	adox(	48,(up), w0)
	mulx(	-8,(dp), w2, w3)
	mov	w0, 48(up)
L(b3x):	lea	64(up), up
	adcx(	w1, w2)
	mulx(	(dp), w0, w1)
	jmp	L(top)

L(done):mov	%r13, %rax
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	FUNC_EXIT()
	ret

L(sma):
ifdef(`PIC',
`	movslq	28(%r10,dn_param,4), %rax
	lea	(%rax,%r10), jaddr
',`
	mov	56(%r10,dn_param,8), jaddr
')
	jmp	*jaddr

L(1):	mov	(dp_param), %r10
	xor	R32(%rax), R32(%rax)
	mov	(up), %rdx
	dec	un
	mov	%rdx, %r9
L(o1):	mulx(	dinv, %rdx, %r11)	C next quotient
	lea	8(up), up
	mulx(	%r10, %rcx, %rdx)	C 0 1
	add	%r9, %rcx		C 0
	adc	%rax, %rdx		C 1
	add	(up), %rdx		C 1
	setc	R8(%rax)		C 2
	mov	%rdx, %r9		C 1
	dec	un
	jnz	L(o1)
	mov	%r9, (up)

	FUNC_EXIT()
	ret

ifdef(`VER',,`define(`VER',1)')
L(2):	push	%r12
	push	%r14

	mov	dp_param, dp		C free up rdx
	sub	dn_param, un		C loop count
	mov	(up), q
	imul	dinv, q

ifelse(VER,0,`
	xor	R32(%rax), R32(%rax)
L(o2):	test	%eax, %eax		C clear CF and OF
	mulx(	(dp), w2, w3)		C 0 1
	mulx(	8,(dp), %rdx, w1)		C 1 2
	add	(up), w2		C 0
	adc	8(up), %rdx		C 1
	adc	$0, w1			C 2 cannot carry further
	add	w3, %rdx			C 1
	mov	%rdx, 8(up)		C 1
	adc	$0, w1			C 2
	imul	dinv, q			C
	bt	$0, R32(%rax)
	adc	16(up), w1		C 2
	mov	w1, 16(up)
	setc	R8(%rax)
	lea	8(up), up
	dec	un
	jnz	L(o2)
')
ifelse(VER,1,`
	push	%rbx
	push	%r13
	xor	R32(%r13), R32(%r13)
	mov	(up), %rax
	mov	8(up), %rbx
L(o2):	xor	R32(%rcx), R32(%rcx)
	mulx(	(dp), w2, w3)		C 0 1
	mulx(	8,(dp), %rdx, w1)	C 1 2
	adox(	%rax, w2)		C 0
	adcx(	w3, %rdx)		C 1
	adox(	%rbx, %rdx)		C 1
	adox(	%rcx, w1)		C 2 cannot carry further
	mov	%rdx, %rax		C 1
	adc	%rcx, w1		C 2
	imul	dinv, q			C
	bt	$0, R32(%r13)
	adc	16(up), w1		C 2
	mov	w1, %rbx
	setc	R8(%r13)
	lea	8(up), up
	dec	un
	jnz	L(o2)

	mov	%rax, (up)
	mov	%rbx, 8(up)
	mov	%r13, %rax
	pop	%r13
	pop	%rbx
')
ifelse(VER,2,`
	xor	R32(%rax), R32(%rax)
	mov	(up), %r10
	mov	8(up), %r9
L(o2):	mulx(	(dp), %r12, %r11)
	mulx(	8,(dp), %rdx, %rcx)
	add	%r11, %rdx		C 1
	adc	$0, %rcx		C 2
	add	%r10, %r12		C 0  add just to produce carry
	adc	%r9, %rdx		C 1
	mov	%rdx, %r10		C 1
	mulx(	dinv, %rdx, %r12)	C next quotient
	adc	%rax, %rcx		C 2
	setc	R8(%rax)		C 3
	mov	16(up), %r9		C 2
	add	%rcx, %r9		C 2
	adc	$0, R32(%rax)		C 3
	lea	8(up), up
	dec	un
	jnz	L(o2)

	mov	%r10, (up)
	mov	%r9, 8(up)
')
ifelse(VER,3,`
	xor	R32(%rax), R32(%rax)
	mov	(up), %r10
	mov	8(up), %r9
L(o2):	mulx(	(dp), %r12, %r11)
	add	%r10, %r12		C 0  add just to produce carry
	mulx(	8,(dp), %rdx, %rcx)
	adc	%r11, %rdx		C 1
	adc	$0, %rcx		C 2
	add	%r9, %rdx		C 1
	mov	%rdx, %r10		C 1
	mulx(	dinv, %rdx, %r12)	C next quotient
	adc	%rax, %rcx		C 2
	setc	R8(%rax)		C 3
	mov	16(up), %r9		C 2
	add	%rcx, %r9		C 2
	adc	$0, R32(%rax)		C 3
	lea	8(up), up
	dec	un
	jnz	L(o2)

	mov	%r10, (up)
	mov	%r9, 8(up)
')
	pop	%r14
	pop	%r12
	FUNC_EXIT()
	ret

ifelse(eval(MAX_SPECIAL>=3),1,`
L(3):	push	%rbx
	push	%r12
	push	%r13
	push	%r14

	mov	dp_param, dp		C free up rdx
	xor	%r13, %r13
	sub	dn_param, un		C outer loop count
	mov	(up), %rax
	mov	8(up), %rbx
	mov	%rax, q
	imul	dinv, q
L(o3):	xor	R32(%rcx), R32(%rcx)	C clear rcx, CF, and OF
	mulx(	(dp), w0, w1)		C 0 1
	adox(	%rax, w0)		C 0
	mulx(	8,(dp), %rax, w3)	C 1 2
	adcx(	w1, %rax)		C 1
	adox(	%rbx, %rax)		C 1
	mulx(	16,(dp), %rbx, w1)	C 2 3
	mov	dinv, q			C 1
	mulx(	%rax, q, w0)
	adcx(	w3, %rbx)		C 2
	adox(	16,(up), %rbx)		C 2
	adox(	%rcx, w1)		C 3
	adc	$0, w1			C 3
	bt	$0, R32(%r13)
	adc	w1, 24(up)
	setc	R8(%r13)
	lea	8(up), up
	dec	un
	jnz	L(o3)
	jmp	L(esma)
')

ifelse(eval(MAX_SPECIAL>=4),1,`
L(4):	push	%rbx
	push	%r12
	push	%r13
	push	%r14

	mov	dp_param, dp		C free up rdx
	xor	%r13, %r13
	sub	dn_param, un		C outer loop count
	mov	(up), %rax
	mov	8(up), %rbx
	mov	%rax, q
	imul	dinv, q
L(o4):	xor	R32(%rcx), R32(%rcx)
	mulx(	(dp), w2, w3)
	adox(	%rax, w2)
	mulx(	8,(dp), %rax, w1)
	adcx(	w3, %rax)
	adox(	%rbx, %rax)
	mulx(	16,(dp), %rbx, w3)
	adcx(	w1, %rbx)
	mulx(	24,(dp), w0, w1)
	mov	dinv, q
	mulx(	%rax, q, w2)
	adox(	16,(up), %rbx)
	adcx(	w3, w0)
	adox(	24,(up), w0)
	adox(	%rcx, w1)
	mov	w0, 24(up)
	adc	%rcx, w1
	bt	$0, R32(%r13)
	adc	w1, 32(up)
	setc	R8(%r13)
	lea	8(up), up
	dec	un
	jnz	L(o4)
	jmp	L(esma)
')

ifelse(eval(MAX_SPECIAL>=5),1,`
L(5):	push	%rbx
	push	%r12
	push	%r13
	push	%r14

	mov	dp_param, dp		C free up rdx
	xor	%r13, %r13
	sub	dn_param, un		C outer loop count
	mov	(up), %rax
	mov	8(up), %rbx
	mov	%rax, q
	imul	dinv, q
L(o5):	xor	R32(%rcx), R32(%rcx)
	mulx(	(dp), w0, w1)
	adox(	%rax, w0)
	mulx(	8,(dp), %rax, w3)
	adcx(	w1, %rax)
	adox(	%rbx, %rax)
	mulx(	16,(dp), %rbx, w1)
	adcx(	w3, %rbx)
	adox(	16,(up), %rbx)
	mulx(	24,(dp), w2, w3)
	adcx(	w1, w2)
	mulx(	32,(dp), w0, w1)
	adox(	24,(up), w2)
	adcx(	w3, w0)
	mov	dinv, q
	mulx(	%rax, q, w3)
	mov	w2, 24(up)
	adox(	32,(up), w0)
	adox(	%rcx, w1)
	mov	w0, 32(up)
	adc	%rcx, w1
	bt	$0, R32(%r13)
	adc	w1, 40(up)
	setc	R8(%r13)
	lea	8(up), up
	dec	un
	jnz	L(o5)
	jmp	L(esma)
')

ifelse(eval(MAX_SPECIAL>=6),1,`
L(6):	push	%rbx
	push	%r12
	push	%r13
	push	%r14

	mov	dp_param, dp		C free up rdx
	xor	%r13, %r13
	sub	dn_param, un		C outer loop count
	mov	(up), %rax
	mov	8(up), %rbx
	mov	%rax, q
	imul	dinv, q
L(o6):	xor	R32(%rcx), R32(%rcx)
	mulx(	(dp), w2, w3)
	adox(	%rax, w2)
	mulx(	8,(dp), %rax, w1)
	adcx(	w3, %rax)
	adox(	%rbx, %rax)
	mulx(	16,(dp), %rbx, w3)
	adcx(	w1, %rbx)
	mulx(	24,(dp), w0, w1)
	adox(	16,(up), %rbx)
	adcx(	w3, w0)
	adox(	24,(up), w0)
	mulx(	32,(dp), w2, w3)
	mov	w0, 24(up)
	adcx(	w1, w2)
	mulx(	40,(dp), w0, w1)
	adox(	32,(up), w2)
	adcx(	w3, w0)
	mov	dinv, q
	mulx(	%rax, q, w3)
	mov	w2, 32(up)
	adox(	40,(up), w0)
	adox(	%rcx, w1)
	mov	w0, 40(up)
	adc	%rcx, w1
	bt	$0, R32(%r13)
	adc	w1, 48(up)
	setc	R8(%r13)
	lea	8(up), up
	dec	un
	jnz	L(o6)
	jmp	L(esma)
')

ifelse(eval(MAX_SPECIAL>=7),1,`
L(7):	push	%rbx
	push	%r12
	push	%r13
	push	%r14

	mov	dp_param, dp
	xor	%r13, %r13
	sub	dn_param, un
	mov	(up), %rax
	mov	8(up), %rbx
	mov	%rax, q
	imul	dinv, q
L(o7):	xor	R32(%rcx), R32(%rcx)
	mulx(	(dp), w0, w1)
	adox(	%rax, w0)
	mulx(	8,(dp), %rax, w3)
	adcx(	w1, %rax)
	adox(	%rbx, %rax)
	mulx(	16,(dp), %rbx, w1)
	adcx(	w3, %rbx)
	mulx(	24,(dp), w2, w3)
	adcx(	w1, w2)
	adox(	16,(up), %rbx)
	mulx(	32,(dp), w0, w1)
	adox(	24,(up), w2)
	adcx(	w3, w0)
	mov	w2, 24(up)
	adox(	32,(up), w0)
	mulx(	40,(dp), w2, w3)
	mov	w0, 32(up)
	adcx(	w1, w2)
	mulx(	48,(dp), w0, w1)
	adox(	40,(up), w2)
	adcx(	w3, w0)
	mov	w2, 40(up)
	mov	%rax, q
	mulx(	dinv, q, w2)
	adox(	48,(up), w0)
	adox(	%rcx, w1)
	mov	w0, 48(up)
	adc	%rcx, w1
	bt	$0, R32(%r13)
	adc	w1, 56(up)
	setc	R8(%r13)
	lea	8(up), up
	dec	un
	jnz	L(o7)
	jmp	L(esma)
')

ifelse(eval(MAX_SPECIAL>=8),1,`
L(8):	push	%rbx
	push	%r12
	push	%r13
	push	%r14

	mov	dp_param, dp
	xor	%r13, %r13
	sub	dn_param, un
	mov	(up), %rax
	mov	8(up), %rbx
	mov	%rax, q
	imul	dinv, q
L(o8):	xor	R32(%rcx), R32(%rcx)
	mulx(	(dp), w2, w3)
	adox(	%rax, w2)
	mulx(	8,(dp), %rax, w1)
	adcx(	w3, %rax)
	adox(	%rbx, %rax)
	mulx(	16,(dp), %rbx, w3)
	adcx(	w1, %rbx)
	mulx(	24,(dp), w0, w1)
	adox(	16,(up), %rbx)
	adcx(	w3, w0)
	mulx(	32,(dp), w2, w3)
	adcx(	w1, w2)
	adox(	24,(up), w0)
	mov	w0, 24(up)
	mulx(	40,(dp), w0, w1)
	adox(	32,(up), w2)
	adcx(	w3, w0)
	mov	w2, 32(up)
	adox(	40,(up), w0)
	mulx(	48,(dp), w2, w3)
	mov	w0, 40(up)
	adcx(	w1, w2)
	mulx(	56,(dp), w0, w1)
	adox(	48,(up), w2)
	adcx(	w3, w0)
	mov	dinv, q
	mulx(	%rax, q, w3)
	mov	w2, 48(up)
	adox(	56,(up), w0)
	adox(	%rcx, w1)
	mov	w0, 56(up)
	adc	%rcx, w1
	bt	$0, R32(%r13)
	adc	w1, 64(up)
	setc	R8(%r13)
	lea	8(up), up
	dec	un
	jnz	L(o8)
	jmp	L(esma)
')

L(esma):mov	%rax, (up)
	mov	%rbx, 8(up)
	mov	%r13, %rax
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	FUNC_EXIT()
	ret


	JUMPTABSECT
	ALIGN(8)
L(atab):JMPENT(	L(f0), L(atab))
	JMPENT(	L(f1), L(atab))
	JMPENT(	L(f2), L(atab))
	JMPENT(	L(f3), L(atab))
	JMPENT(	L(f4), L(atab))
	JMPENT(	L(f5), L(atab))
	JMPENT(	L(f6), L(atab))
	JMPENT(	L(f7), L(atab))
	JMPENT(	L(1), L(atab))
	JMPENT(	L(2), L(atab))
	JMPENT(	L(3), L(atab))
	JMPENT(	L(4), L(atab))
	JMPENT(	L(5), L(atab))
	JMPENT(	L(6), L(atab))
	JMPENT(	L(7), L(atab))
	JMPENT(	L(8), L(atab))
	TEXT
EPILOGUE()
