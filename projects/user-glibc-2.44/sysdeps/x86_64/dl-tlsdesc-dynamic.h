/* Thread-local storage handling in the ELF dynamic linker.  x86_64 version.
   Copyright (C) 2004-2026 Free Software Foundation, Inc.
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
   <https://www.gnu.org/licenses/>.  */

#ifndef SECTION
# define SECTION(p)	p
#endif

#undef REGISTER_SAVE_AREA
#undef LOCAL_STORAGE_AREA
#undef BASE

#include "dl-trampoline-state.h"

	.section SECTION(.text),"ax",@progbits

	.hidden _dl_tlsdesc_dynamic
	.global	_dl_tlsdesc_dynamic
	.type	_dl_tlsdesc_dynamic,@function

     /* %rax points to the TLS descriptor, such that 0(%rax) points to
	_dl_tlsdesc_dynamic itself, and 8(%rax) points to a struct
	tlsdesc_dynamic_arg object.  It must return in %rax the offset
	between the thread pointer and the object denoted by the
	argument, without clobbering any registers.

	The assembly code that follows is a rendition of the following
	C code, hand-optimized a little bit.

ptrdiff_t
_dl_tlsdesc_dynamic (register struct tlsdesc *tdp asm ("%rax"))
{
  struct tlsdesc_dynamic_arg *td = tdp->arg;
  dtv_t *dtv = *(dtv_t **)((char *)__thread_pointer + DTV_OFFSET);
  if (__builtin_expect (td->gen_count <= dtv[0].counter
			&& (dtv[td->tlsinfo.ti_module].pointer.val
			    != TLS_DTV_UNALLOCATED),
			1))
    return dtv[td->tlsinfo.ti_module].pointer.val + td->tlsinfo.ti_offset
      - __thread_pointer;

  return __tls_get_addr_internal (&td->tlsinfo) - __thread_pointer;
}
*/
	cfi_startproc
	.align 16
_dl_tlsdesc_dynamic:
	_CET_ENDBR
	/* Preserve call-clobbered registers that we modify.
	   We need two scratch regs anyway.  */
	movq	%rsi, -16(%rsp)
	mov	%fs:DTV_OFFSET, %RSI_LP
	movq	%rdi, -8(%rsp)
	movq	TLSDESC_ARG(%rax), %rdi
	movq	(%rsi), %rax
	cmpq	%rax, TLSDESC_GEN_COUNT(%rdi)
	ja	2f
	movq	TLSDESC_MODID(%rdi), %rax
	salq	$4, %rax
	movq	(%rax,%rsi), %rax
	cmpq	$-1, %rax
	je	2f
	addq	TLSDESC_MODOFF(%rdi), %rax
1:
	movq	-16(%rsp), %rsi
	sub	%fs:0, %RAX_LP
	movq	-8(%rsp), %rdi
	ret
2:
#if DL_RUNTIME_RESOLVE_REALIGN_STACK
	movq	%rbx, -24(%rsp)
	mov	%RSP_LP, %RBX_LP
	cfi_def_cfa_register(%rbx)
	and	$-STATE_SAVE_ALIGNMENT, %RSP_LP
#endif
#ifdef REGISTER_SAVE_AREA
# if DL_RUNTIME_RESOLVE_REALIGN_STACK
	/* STATE_SAVE_OFFSET has space for 8 integer registers.  But we
	   need space for RCX, RDX, RSI, RDI, R8, R9, R10 and R11, plus
	   RBX above.  */
	sub	$(REGISTER_SAVE_AREA + STATE_SAVE_ALIGNMENT), %RSP_LP
# else
	sub	$REGISTER_SAVE_AREA, %RSP_LP
	cfi_adjust_cfa_offset(REGISTER_SAVE_AREA)
# endif
#else
	/* Allocate stack space of the required size to save the state.  */
	sub	_dl_x86_features_tlsdesc_state_size(%rip), %RSP_LP
#endif
	/* Besides rdi and rsi, saved above, save rcx, rdx, r8, r9,
	   r10 and r11.  */
	movq	%rcx, REGISTER_SAVE_RCX(%rsp)
	movq	%rdx, REGISTER_SAVE_RDX(%rsp)
	movq	%r8, REGISTER_SAVE_R8(%rsp)
	movq	%r9, REGISTER_SAVE_R9(%rsp)
	movq	%r10, REGISTER_SAVE_R10(%rsp)
	movq	%r11, REGISTER_SAVE_R11(%rsp)
#ifdef USE_FXSAVE
	fxsave	STATE_SAVE_OFFSET(%rsp)
#else
	movl	$TLSDESC_CALL_STATE_SAVE_MASK, %eax
	xorl	%edx, %edx
	/* Clear the XSAVE Header.  */
# ifdef USE_XSAVE
	movq	%rdx, (STATE_SAVE_OFFSET + 512)(%rsp)
	movq	%rdx, (STATE_SAVE_OFFSET + 512 + 8)(%rsp)
# endif
	movq	%rdx, (STATE_SAVE_OFFSET + 512 + 8 * 2)(%rsp)
	movq	%rdx, (STATE_SAVE_OFFSET + 512 + 8 * 3)(%rsp)
	movq	%rdx, (STATE_SAVE_OFFSET + 512 + 8 * 4)(%rsp)
	movq	%rdx, (STATE_SAVE_OFFSET + 512 + 8 * 5)(%rsp)
	movq	%rdx, (STATE_SAVE_OFFSET + 512 + 8 * 6)(%rsp)
	movq	%rdx, (STATE_SAVE_OFFSET + 512 + 8 * 7)(%rsp)
# ifdef USE_XSAVE
	xsave	STATE_SAVE_OFFSET(%rsp)
# else
	xsavec	STATE_SAVE_OFFSET(%rsp)
# endif
#endif
	/* %rdi already points to the tlsinfo data structure.  */
	call	HIDDEN_JUMPTARGET (__tls_get_addr)
	# Get register content back.
#ifdef USE_FXSAVE
	fxrstor	STATE_SAVE_OFFSET(%rsp)
#else
	/* Save and retore __tls_get_addr return value stored in RAX.  */
	mov	%RAX_LP, %RCX_LP
	movl	$TLSDESC_CALL_STATE_SAVE_MASK, %eax
	xorl	%edx, %edx
	xrstor	STATE_SAVE_OFFSET(%rsp)
	mov	%RCX_LP, %RAX_LP
#endif
	movq	REGISTER_SAVE_R11(%rsp), %r11
	movq	REGISTER_SAVE_R10(%rsp), %r10
	movq	REGISTER_SAVE_R9(%rsp), %r9
	movq	REGISTER_SAVE_R8(%rsp), %r8
	movq	REGISTER_SAVE_RDX(%rsp), %rdx
	movq	REGISTER_SAVE_RCX(%rsp), %rcx
#if DL_RUNTIME_RESOLVE_REALIGN_STACK
	mov	%RBX_LP, %RSP_LP
	cfi_def_cfa_register(%rsp)
	movq	-24(%rsp), %rbx
	cfi_restore(%rbx)
#else
	add	$REGISTER_SAVE_AREA, %RSP_LP
	cfi_adjust_cfa_offset(-REGISTER_SAVE_AREA)
#endif
	jmp	1b
	cfi_endproc
	.size	_dl_tlsdesc_dynamic, .-_dl_tlsdesc_dynamic

#undef STATE_SAVE_ALIGNMENT
