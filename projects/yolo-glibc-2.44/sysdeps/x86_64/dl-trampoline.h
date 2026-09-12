/* PLT trampolines.  x86-64 version.
   Copyright (C) 2009-2026 Free Software Foundation, Inc.
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

	.section SECTION(.text),"ax",@progbits
#ifdef _dl_runtime_resolve

# undef REGISTER_SAVE_AREA
# undef LOCAL_STORAGE_AREA
# undef BASE

# include "dl-trampoline-state.h"

	.globl _dl_runtime_resolve
	.hidden _dl_runtime_resolve
	.type _dl_runtime_resolve, @function
	.align 16
	cfi_startproc
_dl_runtime_resolve:
	cfi_adjust_cfa_offset(16) # Incorporate PLT
	_CET_ENDBR
# if DL_RUNTIME_RESOLVE_REALIGN_STACK
#  if LOCAL_STORAGE_AREA != 8
#   error LOCAL_STORAGE_AREA must be 8
#  endif
	pushq %rbx			# push subtracts stack by 8.
	cfi_adjust_cfa_offset(8)
	cfi_rel_offset(%rbx, 0)
	mov %RSP_LP, %RBX_LP
	cfi_def_cfa_register(%rbx)
	and $-STATE_SAVE_ALIGNMENT, %RSP_LP
# endif
# ifdef REGISTER_SAVE_AREA
	sub $REGISTER_SAVE_AREA, %RSP_LP
#  if !DL_RUNTIME_RESOLVE_REALIGN_STACK
	cfi_adjust_cfa_offset(REGISTER_SAVE_AREA)
#  endif
# else
	# Allocate stack space of the required size to save the state.
#  if IS_IN (rtld)
	sub _rtld_local_ro+RTLD_GLOBAL_RO_DL_X86_CPU_FEATURES_OFFSET+XSAVE_STATE_SIZE_OFFSET(%rip), %RSP_LP
#  else
	sub _dl_x86_cpu_features+XSAVE_STATE_SIZE_OFFSET(%rip), %RSP_LP
#  endif
# endif
	# Preserve registers otherwise clobbered.
	movq %rax, REGISTER_SAVE_RAX(%rsp)
	movq %rcx, REGISTER_SAVE_RCX(%rsp)
	movq %rdx, REGISTER_SAVE_RDX(%rsp)
	movq %rsi, REGISTER_SAVE_RSI(%rsp)
	movq %rdi, REGISTER_SAVE_RDI(%rsp)
	movq %r8, REGISTER_SAVE_R8(%rsp)
	movq %r9, REGISTER_SAVE_R9(%rsp)
# ifdef USE_FXSAVE
	fxsave STATE_SAVE_OFFSET(%rsp)
# else
	movl $STATE_SAVE_MASK, %eax
	xorl %edx, %edx
	# Clear the XSAVE Header.
#  ifdef USE_XSAVE
	movq %rdx, (STATE_SAVE_OFFSET + 512)(%rsp)
	movq %rdx, (STATE_SAVE_OFFSET + 512 + 8)(%rsp)
#  endif
	movq %rdx, (STATE_SAVE_OFFSET + 512 + 8 * 2)(%rsp)
	movq %rdx, (STATE_SAVE_OFFSET + 512 + 8 * 3)(%rsp)
	movq %rdx, (STATE_SAVE_OFFSET + 512 + 8 * 4)(%rsp)
	movq %rdx, (STATE_SAVE_OFFSET + 512 + 8 * 5)(%rsp)
	movq %rdx, (STATE_SAVE_OFFSET + 512 + 8 * 6)(%rsp)
	movq %rdx, (STATE_SAVE_OFFSET + 512 + 8 * 7)(%rsp)
#  ifdef USE_XSAVE
	xsave STATE_SAVE_OFFSET(%rsp)
#  else
	xsavec STATE_SAVE_OFFSET(%rsp)
#  endif
# endif
	# Copy args pushed by PLT in register.
	# %rdi: link_map, %rsi: reloc_index
	mov (LOCAL_STORAGE_AREA + 8)(%BASE), %RSI_LP
	mov LOCAL_STORAGE_AREA(%BASE), %RDI_LP
	call _dl_fixup		# Call resolver.
	mov %RAX_LP, %R11_LP	# Save return value
	# Get register content back.
# ifdef USE_FXSAVE
	fxrstor STATE_SAVE_OFFSET(%rsp)
# else
	movl $STATE_SAVE_MASK, %eax
	xorl %edx, %edx
	xrstor STATE_SAVE_OFFSET(%rsp)
# endif
	movq REGISTER_SAVE_R9(%rsp), %r9
	movq REGISTER_SAVE_R8(%rsp), %r8
	movq REGISTER_SAVE_RDI(%rsp), %rdi
	movq REGISTER_SAVE_RSI(%rsp), %rsi
	movq REGISTER_SAVE_RDX(%rsp), %rdx
	movq REGISTER_SAVE_RCX(%rsp), %rcx
	movq REGISTER_SAVE_RAX(%rsp), %rax
# if DL_RUNTIME_RESOLVE_REALIGN_STACK
	mov %RBX_LP, %RSP_LP
	cfi_def_cfa_register(%rsp)
	movq (%rsp), %rbx
	cfi_restore(%rbx)
# endif
	# Adjust stack(PLT did 2 pushes)
	add $(LOCAL_STORAGE_AREA + 16), %RSP_LP
	cfi_adjust_cfa_offset(-(LOCAL_STORAGE_AREA + 16))
	jmp *%r11		# Jump to function address.
	cfi_endproc
	.size _dl_runtime_resolve, .-_dl_runtime_resolve
#endif


#if !defined PROF && defined _dl_runtime_profile
# if (LR_VECTOR_OFFSET % VEC_SIZE) != 0
#  error LR_VECTOR_OFFSET must be multiple of VEC_SIZE
# endif

	.globl _dl_runtime_profile
	.hidden _dl_runtime_profile
	.type _dl_runtime_profile, @function
	.align 16
_dl_runtime_profile:
	cfi_startproc
	cfi_adjust_cfa_offset(16) # Incorporate PLT
	_CET_ENDBR
	/* The La_x86_64_regs data structure pointed to by the
	   fourth parameter must be VEC_SIZE-byte aligned.  This must
	   be explicitly enforced.  We have the set up a dynamically
	   sized stack frame.  %rbx points to the top half which
	   has a fixed size and preserves the original stack pointer.  */

	sub $32, %RSP_LP	# Allocate the local storage.
	cfi_adjust_cfa_offset(32)
	movq %rbx, (%rsp)
	cfi_rel_offset(%rbx, 0)

	/* On the stack:
		56(%rbx)	parameter #1
		48(%rbx)	return address

		40(%rbx)	reloc index
		32(%rbx)	link_map

		24(%rbx)	La_x86_64_regs pointer
		16(%rbx)	framesize
		 8(%rbx)	rax
		  (%rbx)	rbx
	*/

	movq %rax, 8(%rsp)
	mov %RSP_LP, %RBX_LP
	cfi_def_cfa_register(%rbx)

	/* Actively align the La_x86_64_regs structure.  */
	and $-VEC_SIZE, %RSP_LP
	/* sizeof(La_x86_64_regs).  Need extra space for 8 SSE registers
	   to detect if any xmm0-xmm7 registers are changed by audit
	   module.  */
	sub $(LR_SIZE + XMM_SIZE*8), %RSP_LP
	movq %rsp, 24(%rbx)

	/* Fill the La_x86_64_regs structure.  */
	movq %rdx, LR_RDX_OFFSET(%rsp)
	movq %r8,  LR_R8_OFFSET(%rsp)
	movq %r9,  LR_R9_OFFSET(%rsp)
	movq %rcx, LR_RCX_OFFSET(%rsp)
	movq %rsi, LR_RSI_OFFSET(%rsp)
	movq %rdi, LR_RDI_OFFSET(%rsp)
	movq %rbp, LR_RBP_OFFSET(%rsp)

	lea 48(%rbx), %RAX_LP
	movq %rax, LR_RSP_OFFSET(%rsp)

	/* We always store the XMM registers even if AVX is available.
	   This is to provide backward binary compatibility for existing
	   audit modules.  */
	VMOVA %xmm0, (LR_XMM_OFFSET + XMM_SIZE*0)(%rsp)
	VMOVA %xmm1, (LR_XMM_OFFSET + XMM_SIZE*1)(%rsp)
	VMOVA %xmm2, (LR_XMM_OFFSET + XMM_SIZE*2)(%rsp)
	VMOVA %xmm3, (LR_XMM_OFFSET + XMM_SIZE*3)(%rsp)
	VMOVA %xmm4, (LR_XMM_OFFSET + XMM_SIZE*4)(%rsp)
	VMOVA %xmm5, (LR_XMM_OFFSET + XMM_SIZE*5)(%rsp)
	VMOVA %xmm6, (LR_XMM_OFFSET + XMM_SIZE*6)(%rsp)
	VMOVA %xmm7, (LR_XMM_OFFSET + XMM_SIZE*7)(%rsp)

# ifdef RESTORE_AVX
	/* This is to support AVX audit modules.  */
	VMOVA %VEC(0), (LR_VECTOR_OFFSET + VECTOR_SIZE*0)(%rsp)
	VMOVA %VEC(1), (LR_VECTOR_OFFSET + VECTOR_SIZE*1)(%rsp)
	VMOVA %VEC(2), (LR_VECTOR_OFFSET + VECTOR_SIZE*2)(%rsp)
	VMOVA %VEC(3), (LR_VECTOR_OFFSET + VECTOR_SIZE*3)(%rsp)
	VMOVA %VEC(4), (LR_VECTOR_OFFSET + VECTOR_SIZE*4)(%rsp)
	VMOVA %VEC(5), (LR_VECTOR_OFFSET + VECTOR_SIZE*5)(%rsp)
	VMOVA %VEC(6), (LR_VECTOR_OFFSET + VECTOR_SIZE*6)(%rsp)
	VMOVA %VEC(7), (LR_VECTOR_OFFSET + VECTOR_SIZE*7)(%rsp)

	/* Save xmm0-xmm7 registers to detect if any of them are
	   changed by audit module.  */
	vmovdqa %xmm0, (LR_SIZE + XMM_SIZE*0)(%rsp)
	vmovdqa %xmm1, (LR_SIZE + XMM_SIZE*1)(%rsp)
	vmovdqa %xmm2, (LR_SIZE + XMM_SIZE*2)(%rsp)
	vmovdqa %xmm3, (LR_SIZE + XMM_SIZE*3)(%rsp)
	vmovdqa %xmm4, (LR_SIZE + XMM_SIZE*4)(%rsp)
	vmovdqa %xmm5, (LR_SIZE + XMM_SIZE*5)(%rsp)
	vmovdqa %xmm6, (LR_SIZE + XMM_SIZE*6)(%rsp)
	vmovdqa %xmm7, (LR_SIZE + XMM_SIZE*7)(%rsp)
# endif

	mov %RSP_LP, %RCX_LP	# La_x86_64_regs pointer to %rcx.
	mov 48(%rbx), %RDX_LP	# Load return address if needed.
	mov 40(%rbx), %RSI_LP	# Copy args pushed by PLT in register.
	mov 32(%rbx), %RDI_LP	# %rdi: link_map, %rsi: reloc_index
	lea 16(%rbx), %R8_LP	# Address of framesize
	call _dl_profile_fixup	# Call resolver.

	mov %RAX_LP, %R11_LP	# Save return value.

	movq 8(%rbx), %rax	# Get back register content.
	movq LR_RDX_OFFSET(%rsp), %rdx
	movq  LR_R8_OFFSET(%rsp), %r8
	movq  LR_R9_OFFSET(%rsp), %r9

	VMOVA (LR_XMM_OFFSET + XMM_SIZE*0)(%rsp), %xmm0
	VMOVA (LR_XMM_OFFSET + XMM_SIZE*1)(%rsp), %xmm1
	VMOVA (LR_XMM_OFFSET + XMM_SIZE*2)(%rsp), %xmm2
	VMOVA (LR_XMM_OFFSET + XMM_SIZE*3)(%rsp), %xmm3
	VMOVA (LR_XMM_OFFSET + XMM_SIZE*4)(%rsp), %xmm4
	VMOVA (LR_XMM_OFFSET + XMM_SIZE*5)(%rsp), %xmm5
	VMOVA (LR_XMM_OFFSET + XMM_SIZE*6)(%rsp), %xmm6
	VMOVA (LR_XMM_OFFSET + XMM_SIZE*7)(%rsp), %xmm7

# ifdef RESTORE_AVX
	/* Check if any xmm0-xmm7 registers are changed by audit
	   module.  */
	vpcmpeqb (LR_SIZE)(%rsp), %xmm0, %xmm8
	vpmovmskb %xmm8, %esi
	incw %si
	je 2f
	vmovdqa	%xmm0, (LR_VECTOR_OFFSET)(%rsp)
	jmp 1f
2:	VMOVA (LR_VECTOR_OFFSET)(%rsp), %VEC(0)
	vmovdqa	%xmm0, (LR_XMM_OFFSET)(%rsp)

1:	vpcmpeqb (LR_SIZE + XMM_SIZE)(%rsp), %xmm1, %xmm8
	vpmovmskb %xmm8, %esi
	incw %si
	je 2f
	vmovdqa	%xmm1, (LR_VECTOR_OFFSET + VECTOR_SIZE)(%rsp)
	jmp 1f
2:	VMOVA (LR_VECTOR_OFFSET + VECTOR_SIZE)(%rsp), %VEC(1)
	vmovdqa	%xmm1, (LR_XMM_OFFSET + XMM_SIZE)(%rsp)

1:	vpcmpeqb (LR_SIZE + XMM_SIZE*2)(%rsp), %xmm2, %xmm8
	vpmovmskb %xmm8, %esi
	incw %si
	je 2f
	vmovdqa	%xmm2, (LR_VECTOR_OFFSET + VECTOR_SIZE*2)(%rsp)
	jmp 1f
2:	VMOVA (LR_VECTOR_OFFSET + VECTOR_SIZE*2)(%rsp), %VEC(2)
	vmovdqa	%xmm2, (LR_XMM_OFFSET + XMM_SIZE*2)(%rsp)

1:	vpcmpeqb (LR_SIZE + XMM_SIZE*3)(%rsp), %xmm3, %xmm8
	vpmovmskb %xmm8, %esi
	incw %si
	je 2f
	vmovdqa	%xmm3, (LR_VECTOR_OFFSET + VECTOR_SIZE*3)(%rsp)
	jmp 1f
2:	VMOVA (LR_VECTOR_OFFSET + VECTOR_SIZE*3)(%rsp), %VEC(3)
	vmovdqa	%xmm3, (LR_XMM_OFFSET + XMM_SIZE*3)(%rsp)

1:	vpcmpeqb (LR_SIZE + XMM_SIZE*4)(%rsp), %xmm4, %xmm8
	vpmovmskb %xmm8, %esi
	incw %si
	je 2f
	vmovdqa	%xmm4, (LR_VECTOR_OFFSET + VECTOR_SIZE*4)(%rsp)
	jmp 1f
2:	VMOVA (LR_VECTOR_OFFSET + VECTOR_SIZE*4)(%rsp), %VEC(4)
	vmovdqa	%xmm4, (LR_XMM_OFFSET + XMM_SIZE*4)(%rsp)

1:	vpcmpeqb (LR_SIZE + XMM_SIZE*5)(%rsp), %xmm5, %xmm8
	vpmovmskb %xmm8, %esi
	incw %si
	je 2f
	vmovdqa	%xmm5, (LR_VECTOR_OFFSET + VECTOR_SIZE*5)(%rsp)
	jmp 1f
2:	VMOVA (LR_VECTOR_OFFSET + VECTOR_SIZE*5)(%rsp), %VEC(5)
	vmovdqa	%xmm5, (LR_XMM_OFFSET + XMM_SIZE*5)(%rsp)

1:	vpcmpeqb (LR_SIZE + XMM_SIZE*6)(%rsp), %xmm6, %xmm8
	vpmovmskb %xmm8, %esi
	incw %si
	je 2f
	vmovdqa	%xmm6, (LR_VECTOR_OFFSET + VECTOR_SIZE*6)(%rsp)
	jmp 1f
2:	VMOVA (LR_VECTOR_OFFSET + VECTOR_SIZE*6)(%rsp), %VEC(6)
	vmovdqa	%xmm6, (LR_XMM_OFFSET + XMM_SIZE*6)(%rsp)

1:	vpcmpeqb (LR_SIZE + XMM_SIZE*7)(%rsp), %xmm7, %xmm8
	vpmovmskb %xmm8, %esi
	incw %si
	je 2f
	vmovdqa	%xmm7, (LR_VECTOR_OFFSET + VECTOR_SIZE*7)(%rsp)
	jmp 1f
2:	VMOVA (LR_VECTOR_OFFSET + VECTOR_SIZE*7)(%rsp), %VEC(7)
	vmovdqa	%xmm7, (LR_XMM_OFFSET + XMM_SIZE*7)(%rsp)

1:
# endif

	mov  16(%rbx), %RCX_LP	# Anything in framesize?
	test %RCX_LP, %RCX_LP
	jns 3f

	/* There's nothing in the frame size, so there
	   will be no call to the _dl_audit_pltexit. */

	/* Get back registers content.  */
	movq LR_RCX_OFFSET(%rsp), %rcx
	movq LR_RSI_OFFSET(%rsp), %rsi
	movq LR_RDI_OFFSET(%rsp), %rdi

	mov %RBX_LP, %RSP_LP
	movq (%rsp), %rbx
	cfi_restore(%rbx)
	cfi_def_cfa_register(%rsp)

	add $48, %RSP_LP	# Adjust the stack to the return value
				# (eats the reloc index and link_map)
	cfi_adjust_cfa_offset(-48)
	jmp *%r11		# Jump to function address.

3:
	cfi_adjust_cfa_offset(48)
	cfi_rel_offset(%rbx, 0)
	cfi_def_cfa_register(%rbx)

	/* At this point we need to prepare new stack for the function
	   which has to be called.  We copy the original stack to a
	   temporary buffer of the size specified by the 'framesize'
	   returned from _dl_profile_fixup */

	lea LR_RSP_OFFSET(%rbx), %RSI_LP # stack
	add $8, %RCX_LP
	and $-16, %RCX_LP
	sub %RCX_LP, %RSP_LP
	mov %RSP_LP, %RDI_LP
	rep movsb

	movq 24(%rdi), %rcx	# Get back register content.
	movq 32(%rdi), %rsi
	movq 40(%rdi), %rdi

	call *%r11

	mov 24(%rbx), %RSP_LP	# Drop the copied stack content

	/* Now we have to prepare the La_x86_64_retval structure for the
	   _dl_audit_pltexit.  The La_x86_64_regs is being pointed by rsp now,
	   so we just need to allocate the sizeof(La_x86_64_retval) space on
	   the stack, since the alignment has already been taken care of. */
# ifdef RESTORE_AVX
	/* sizeof(La_x86_64_retval).  Need extra space for 2 SSE
	   registers to detect if xmm0/xmm1 registers are changed
	   by audit module.  Since rsp is aligned to VEC_SIZE, we
	   need to make sure that the address of La_x86_64_retval +
	   LRV_VECTOR0_OFFSET is aligned to VEC_SIZE.  */
#  define LRV_SPACE (LRV_SIZE + XMM_SIZE*2)
#  define LRV_MISALIGNED ((LRV_SIZE + LRV_VECTOR0_OFFSET) & (VEC_SIZE - 1))
#  if LRV_MISALIGNED == 0
	sub $LRV_SPACE, %RSP_LP
#  else
	sub $(LRV_SPACE + VEC_SIZE - LRV_MISALIGNED), %RSP_LP
#  endif
# else
	sub $LRV_SIZE, %RSP_LP	# sizeof(La_x86_64_retval)
# endif
	mov %RSP_LP, %RCX_LP	# La_x86_64_retval argument to %rcx.

	/* Fill in the La_x86_64_retval structure.  */
	movq %rax, LRV_RAX_OFFSET(%rcx)
	movq %rdx, LRV_RDX_OFFSET(%rcx)

	VMOVA %xmm0, LRV_XMM0_OFFSET(%rcx)
	VMOVA %xmm1, LRV_XMM1_OFFSET(%rcx)

# ifdef RESTORE_AVX
	/* This is to support AVX audit modules.  */
	VMOVA %VEC(0), LRV_VECTOR0_OFFSET(%rcx)
	VMOVA %VEC(1), LRV_VECTOR1_OFFSET(%rcx)

	/* Save xmm0/xmm1 registers to detect if they are changed
	   by audit module.  */
	vmovdqa %xmm0, (LRV_SIZE + XMM_SIZE*0)(%rcx)
	vmovdqa %xmm1, (LRV_SIZE + XMM_SIZE*1)(%rcx)
# endif

	fstpt LRV_ST0_OFFSET(%rcx)
	fstpt LRV_ST1_OFFSET(%rcx)

	movq 24(%rbx), %rdx	# La_x86_64_regs argument to %rdx.
	movq 40(%rbx), %rsi	# Copy args pushed by PLT in register.
	movq 32(%rbx), %rdi	# %rdi: link_map, %rsi: reloc_index
	call _dl_audit_pltexit

	/* Restore return registers.  */
	movq LRV_RAX_OFFSET(%rsp), %rax
	movq LRV_RDX_OFFSET(%rsp), %rdx

	VMOVA LRV_XMM0_OFFSET(%rsp), %xmm0
	VMOVA LRV_XMM1_OFFSET(%rsp), %xmm1

# ifdef RESTORE_AVX
	/* Check if xmm0/xmm1 registers are changed by audit module.  */
	vpcmpeqb (LRV_SIZE)(%rsp), %xmm0, %xmm2
	vpmovmskb %xmm2, %esi
	incw %si
	jne 1f
	VMOVA LRV_VECTOR0_OFFSET(%rsp), %VEC(0)

1:	vpcmpeqb (LRV_SIZE + XMM_SIZE)(%rsp), %xmm1, %xmm2
	vpmovmskb %xmm2, %esi
	incw %si
	jne 1f
	VMOVA LRV_VECTOR1_OFFSET(%rsp), %VEC(1)

1:
# endif

	fldt LRV_ST1_OFFSET(%rsp)
	fldt LRV_ST0_OFFSET(%rsp)

	mov %RBX_LP, %RSP_LP
	movq (%rsp), %rbx
	cfi_restore(%rbx)
	cfi_def_cfa_register(%rsp)

	add $48, %RSP_LP	# Adjust the stack to the return value
				# (eats the reloc index and link_map)
	cfi_adjust_cfa_offset(-48)
	retq

	cfi_endproc
	.size _dl_runtime_profile, .-_dl_runtime_profile
#endif
