/* Wrapper of i386 ___tls_get_addr to save and restore vector registers.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#if DL_RUNTIME_RESOLVE_REALIGN_STACK
	movl	%ebx, 28(%esp)
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
	subl	RTLD_GLOBAL_RO_DL_X86_CPU_FEATURES_OFFSET \
		+XSAVE_STATE_SIZE_OFFSET+_rtld_local_ro@GOTOFF(%ecx), %esp
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
	call	___tls_get_addr_internal
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
	movl	28(%esp), %ebx
	cfi_restore(%ebx)
#else
	addl	$REGISTER_SAVE_AREA, %esp
	cfi_adjust_cfa_offset(-REGISTER_SAVE_AREA)
#endif

#undef STATE_SAVE_ALIGNMENT
