dnl  x86 __gmpn_addmul_1 (for 386 and 486) -- Multiply a limb vector with a
dnl  limb and add the result to a second limb vector.

dnl  Copyright 1992, 1994, 1997, 1999-2002, 2005 Free Software Foundation, Inc.

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

C			    cycles/limb
C P5				
C P6 model 0-8,10-12		
C P6 model 9  (Banias)		
C P6 model 13 (Dothan)		
C P4 model 0  (Willamette)	
C P4 model 1  (?)		
C P4 model 2  (Northwood)	
C P4 model 3  (Prescott)
C P4 model 4  (Nocona)
C Intel Atom
C AMD K6			
C AMD K7			
C AMD K8			 3.875
C AMD K10


ifdef(`OPERATION_addmul_1',`
      define(ADDSUB,        addl)
      define(M4_function_1,  mpn_addmul_1)
      define(M4_function_1c, mpn_addmul_1c)

',`ifdef(`OPERATION_submul_1',`
      define(ADDSUB,        subl)
      define(M4_function_1,  mpn_submul_1)

',`m4_error(`Need OPERATION_addmul_1 or OPERATION_submul_1
')')')

MULFUNC_PROLOGUE(mpn_addmul_1 mpn_submul_1 mpn_addmul_1c)


C mp_limb_t M4_function_1 (mp_ptr dst, mp_srcptr src, mp_size_t size,
C                          mp_limb_t mult);

define(PARAM_CARRY,      `FRAME+20(%esp)')
define(PARAM_MULTIPLIER, `FRAME+16(%esp)')
define(PARAM_SIZE,       `FRAME+12(%esp)')
define(PARAM_SRC,        `FRAME+8(%esp)')
define(PARAM_DST,        `FRAME+4(%esp)')

	TEXT
	ALIGN(32)
PROLOGUE(M4_function_1)
deflit(`FRAME',0)

	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ebp
deflit(`FRAME',16)

	movl	PARAM_SRC, %esi
	movl	PARAM_SIZE, %ecx
	movl	PARAM_DST, %edi

	movl	(%esi), %eax
	mull	PARAM_MULTIPLIER

	testb	$1, %cl
	jnz	L(bx1)

L(bx0):	movl	%eax, %ebx
	movl	%edx, %ebp
	shrl	$2, %ecx
	jnc	L(lo0)

L(b10):	leal	-8(%esi), %esi
	leal	-8(%edi), %edi
	incl	%ecx
	jmp	L(lo2)

L(bx1):	movl	%eax, %ebp
	movl	%edx, %ebx
	shrl	$2, %ecx
	jc	L(b11)

L(b01):	leal	4(%edi), %edi
	jz	L(end)
	leal	4(%esi), %esi
	jmp	L(top)

L(b11):	leal	-4(%esi), %esi
	leal	-4(%edi), %edi
	incl	%ecx
	jmp	L(lo3)

	ALIGN(16)
L(top):	movl	(%esi), %eax
	mull	PARAM_MULTIPLIER
	ADDSUB	%ebp, -4(%edi)
	adcl	%eax, %ebx
	movl	$0, %ebp
	adcl	%edx, %ebp
L(lo0):	movl	4(%esi), %eax
	mull	PARAM_MULTIPLIER
	ADDSUB	%ebx, (%edi)
	adcl	%eax, %ebp
	movl	$0, %ebx
	adcl	%edx, %ebx
L(lo3):	movl	8(%esi), %eax
	mull	PARAM_MULTIPLIER
	ADDSUB	%ebp, 4(%edi)
	adcl	%eax, %ebx
	movl	$0, %ebp
	adcl	%edx, %ebp
L(lo2):	movl	12(%esi), %eax
	mull	PARAM_MULTIPLIER
	ADDSUB	%ebx, 8(%edi)
	adcl	%eax, %ebp
	movl	$0, %ebx
	adcl	%edx, %ebx

	leal	16(%esi), %esi
	leal	16(%edi), %edi
	decl	%ecx
	jnz	L(top)

L(end):	xor	%eax, %eax
	ADDSUB	%ebp, -4(%edi)
	adcl	%ebx, %eax
	popl	%ebp
	popl	%ebx
	popl	%esi
	popl	%edi
	ret

EPILOGUE()
ifdef(`OPERATION_addmul_1',`
	ALIGN(32)
PROLOGUE(M4_function_1c)
deflit(`FRAME',0)

	pushl	%edi
	pushl	%esi
	pushl	%ebx
	pushl	%ebp
deflit(`FRAME',16)

	movl	PARAM_SRC, %esi
	movl	PARAM_SIZE, %ecx
	movl	PARAM_DST, %edi

	movl	(%esi), %eax
	mull	PARAM_MULTIPLIER

	testb	$1, %cl
	jnz	L(cx1)

	movl	PARAM_CARRY, %ebx
	xorl	%ebp, %ebp

L(cx0):	addl	%eax, %ebx
	adcl	%edx, %ebp
	shrl	$2, %ecx
	jnc	L(lo0)

L(c10):	leal	-8(%esi), %esi
	leal	-8(%edi), %edi
	incl	%ecx
	jmp	L(lo2)

L(cx1):	movl	PARAM_CARRY, %ebp
	xorl	%ebx, %ebx

	addl	%eax, %ebp
	adcl	%edx, %ebx
	shrl	$2, %ecx
	jc	L(c11)

L(c01):	leal	4(%edi), %edi
	jz	L(end)
	leal	4(%esi), %esi
	jmp	L(top)

L(c11):	leal	-4(%esi), %esi
	leal	-4(%edi), %edi
	incl	%ecx
	jmp	L(lo3)
EPILOGUE()
')
