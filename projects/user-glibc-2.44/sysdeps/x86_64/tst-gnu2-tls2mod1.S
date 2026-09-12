/* Check if TLSDESC relocation preserves %rdi, %rsi and %rbx.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <sysdep.h>

/* On AVX512 machines, OFFSET == 40 caused _dl_tlsdesc_dynamic_xsavec
   to clobber %rdi, %rsi and %rbx.  On Intel AVX CPUs, the state size
   is 960 bytes and this test didn't fail.  It may be due to the unused
   last 128 bytes.  On AMD AVX CPUs, the state size is 832 bytes and
   this test might fail without the fix.  */
#ifndef OFFSET
# define OFFSET 40
#endif

	.text
	.p2align 4
	.globl	apply_tls
	.type	apply_tls, @function
apply_tls:
	cfi_startproc
	_CET_ENDBR
	pushq	%rbp
	cfi_def_cfa_offset (16)
	cfi_offset (6, -16)
	movdqu	(%RDI_LP), %xmm0
	lea	tls_var1@TLSDESC(%rip), %RAX_LP
	mov	%RSP_LP, %RBP_LP
	cfi_def_cfa_register (6)
	/* Align stack to 64 bytes.  */
	and	$-64, %RSP_LP
	sub	$OFFSET, %RSP_LP
	pushq	%rbx
	/* Set %ebx to 0xbadbeef.  */
	movl	$0xbadbeef, %ebx
	movl	$0xbadbeef, %esi
	movq	%rdi, saved_rdi(%rip)
	movq	%rsi, saved_rsi(%rip)
	call	*tls_var1@TLSCALL(%RAX_LP)
	/* Check if _dl_tlsdesc_dynamic preserves %rdi, %rsi and %rbx.  */
	cmpq	saved_rdi(%rip), %rdi
	jne	L(hlt)
	cmpq	saved_rsi(%rip), %rsi
	jne	L(hlt)
	cmpl	$0xbadbeef, %ebx
	jne	L(hlt)
	add	%fs:0, %RAX_LP
	movups	%xmm0, 32(%RAX_LP)
	movdqu	16(%RDI_LP), %xmm1
	mov	%RAX_LP, %RBX_LP
	movups	%xmm1, 48(%RAX_LP)
	lea	32(%RBX_LP), %RAX_LP
	pop	%rbx
	leave
	cfi_def_cfa (7, 8)
	ret
L(hlt):
	hlt
	cfi_endproc
	.size	apply_tls, .-apply_tls
	.hidden	tls_var1
	.globl	tls_var1
	.section	.tbss,"awT",@nobits
	.align 16
	.type	tls_var1, @object
	.size	tls_var1, 3200
tls_var1:
	.zero	3200
	.local	saved_rdi
	.comm	saved_rdi,8,8
	.local	saved_rsi
	.comm	saved_rsi,8,8
	.section	.note.GNU-stack,"",@progbits
