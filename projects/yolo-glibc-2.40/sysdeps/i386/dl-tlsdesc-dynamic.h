/* Thread-local storage handling in the ELF dynamic linker.  i386 version.
   Copyright (C) 2004-2024 Free Software Foundation, Inc.
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

#undef REGISTER_SAVE_AREA

#if !defined USE_FNSAVE && (STATE_SAVE_ALIGNMENT % 16) != 0
# error STATE_SAVE_ALIGNMENT must be multiple of 16
#endif

#if DL_RUNTIME_RESOLVE_REALIGN_STACK
# ifdef USE_FNSAVE
#  error USE_FNSAVE shouldn't be defined
# endif
# ifdef USE_FXSAVE
/* Use fxsave to save all registers.  */
#  define REGISTER_SAVE_AREA	512
# endif
#else
# ifdef USE_FNSAVE
/* Use fnsave to save x87 FPU stack registers.  */
#  define REGISTER_SAVE_AREA	108
# else
#  ifndef USE_FXSAVE
#   error USE_FXSAVE must be defined
#  endif
/* Use fxsave to save all registers.  Add 12 bytes to align the stack
   to 16 bytes.  */
#  define REGISTER_SAVE_AREA	(512 + 12)
# endif
#endif

	.hidden _dl_tlsdesc_dynamic
	.global	_dl_tlsdesc_dynamic
	.type	_dl_tlsdesc_dynamic,@function

     /* This function is used for symbols that need dynamic TLS.

	%eax points to the TLS descriptor, such that 0(%eax) points to
	_dl_tlsdesc_dynamic itself, and 4(%eax) points to a struct
	tlsdesc_dynamic_arg object.  It must return in %eax the offset
	between the thread pointer and the object denoted by the
	argument, without clobbering any registers.

	The assembly code that follows is a rendition of the following
	C code, hand-optimized a little bit.

ptrdiff_t
__attribute__ ((__regparm__ (1)))
_dl_tlsdesc_dynamic (struct tlsdesc *tdp)
{
  struct tlsdesc_dynamic_arg *td = tdp->arg;
  dtv_t *dtv = *(dtv_t **)((char *)__thread_pointer + DTV_OFFSET);
  if (__builtin_expect (td->gen_count <= dtv[0].counter
			&& (dtv[td->tlsinfo.ti_module].pointer.val
			    != TLS_DTV_UNALLOCATED),
			1))
    return dtv[td->tlsinfo.ti_module].pointer.val + td->tlsinfo.ti_offset
      - __thread_pointer;

  return ___tls_get_addr (&td->tlsinfo) - __thread_pointer;
}
*/
	cfi_startproc
	.align 16
_dl_tlsdesc_dynamic:
	/* Like all TLS resolvers, preserve call-clobbered registers.
	   We need two scratch regs anyway.  */
	subl	$32, %esp
	cfi_adjust_cfa_offset (32)
	movl	%ecx, 20(%esp)
	movl	%edx, 24(%esp)
	movl	TLSDESC_ARG(%eax), %eax
	movl	%gs:DTV_OFFSET, %edx
	movl	TLSDESC_GEN_COUNT(%eax), %ecx
	cmpl	(%edx), %ecx
	ja	2f
	movl	TLSDESC_MODID(%eax), %ecx
	movl	(%edx,%ecx,8), %edx
	cmpl	$-1, %edx
	je	2f
	movl	TLSDESC_MODOFF(%eax), %eax
	addl	%edx, %eax
1:
	movl	20(%esp), %ecx
	subl	%gs:0, %eax
	movl	24(%esp), %edx
	addl	$32, %esp
	cfi_adjust_cfa_offset (-32)
	ret
	.p2align 4,,7
2:
	cfi_adjust_cfa_offset (32)
#if DL_RUNTIME_RESOLVE_REALIGN_STACK
	movl	%ebx, -28(%esp)
	movl	%esp, %ebx
	cfi_def_cfa_register(%ebx)
	and	$-STATE_SAVE_ALIGNMENT, %esp
#endif
#ifdef REGISTER_SAVE_AREA
	subl	$REGISTER_SAVE_AREA, %esp
# if !DL_RUNTIME_RESOLVE_REALIGN_STACK
	cfi_adjust_cfa_offset(REGISTER_SAVE_AREA)
# endif
#else
# if !DL_RUNTIME_RESOLVE_REALIGN_STACK
#  error DL_RUNTIME_RESOLVE_REALIGN_STACK must be true
# endif
	/* Allocate stack space of the required size to save the state.  */
	LOAD_PIC_REG (cx)
	subl	RTLD_GLOBAL_RO_DL_X86_CPU_FEATURES_OFFSET+XSAVE_STATE_SIZE_OFFSET+_rtld_local_ro@GOTOFF(%ecx), %esp
#endif
#ifdef USE_FNSAVE
	fnsave	(%esp)
#elif defined USE_FXSAVE
	fxsave	(%esp)
#else
	/* Save the argument for ___tls_get_addr in EAX.  */
	movl	%eax, %ecx
	movl	$TLSDESC_CALL_STATE_SAVE_MASK, %eax
	xorl	%edx, %edx
	/* Clear the XSAVE Header.  */
# ifdef USE_XSAVE
	movl	%edx, (512)(%esp)
	movl	%edx, (512 + 4 * 1)(%esp)
	movl	%edx, (512 + 4 * 2)(%esp)
	movl	%edx, (512 + 4 * 3)(%esp)
# endif
	movl	%edx, (512 + 4 * 4)(%esp)
	movl	%edx, (512 + 4 * 5)(%esp)
	movl	%edx, (512 + 4 * 6)(%esp)
	movl	%edx, (512 + 4 * 7)(%esp)
	movl	%edx, (512 + 4 * 8)(%esp)
	movl	%edx, (512 + 4 * 9)(%esp)
	movl	%edx, (512 + 4 * 10)(%esp)
	movl	%edx, (512 + 4 * 11)(%esp)
	movl	%edx, (512 + 4 * 12)(%esp)
	movl	%edx, (512 + 4 * 13)(%esp)
	movl	%edx, (512 + 4 * 14)(%esp)
	movl	%edx, (512 + 4 * 15)(%esp)
# ifdef USE_XSAVE
	xsave	(%esp)
# else
	xsavec	(%esp)
# endif
	/* Restore the argument for ___tls_get_addr in EAX.  */
	movl	%ecx, %eax
#endif
	call	HIDDEN_JUMPTARGET (___tls_get_addr)
	/* Get register content back.  */
#ifdef USE_FNSAVE
	frstor	(%esp)
#elif defined USE_FXSAVE
	fxrstor	(%esp)
#else
	/* Save and retore ___tls_get_addr return value stored in EAX.  */
	movl	%eax, %ecx
	movl	$TLSDESC_CALL_STATE_SAVE_MASK, %eax
	xorl	%edx, %edx
	xrstor	(%esp)
	movl	%ecx, %eax
#endif
#if DL_RUNTIME_RESOLVE_REALIGN_STACK
	mov	%ebx, %esp
	cfi_def_cfa_register(%esp)
	movl	-28(%esp), %ebx
	cfi_restore(%ebx)
#else
	addl	$REGISTER_SAVE_AREA, %esp
	cfi_adjust_cfa_offset(-REGISTER_SAVE_AREA)
#endif
	jmp	1b
	cfi_endproc
	.size	_dl_tlsdesc_dynamic, .-_dl_tlsdesc_dynamic

#undef STATE_SAVE_ALIGNMENT
