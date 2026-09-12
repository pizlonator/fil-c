/* PLT trampolines.  ARM version.
   Copyright (C) 2005-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

/* ??? Needs more rearrangement for the LDM to handle thumb mode.  */
#define NO_THUMB
#include <sysdep.h>
#include <libc-symbols.h>
#include <rtld-global-offsets.h>

	.text
	.globl _dl_runtime_resolve
	.type _dl_runtime_resolve, #function
	CFI_SECTIONS
	cfi_startproc
	.align 2
_dl_runtime_resolve:
	cfi_adjust_cfa_offset (4)
	cfi_rel_offset (lr, 0)

	@ we get called with
	@ 	stack[0] contains the return address from this call
	@	ip contains &GOT[n+3] (pointer to function)
	@	lr points to &GOT[2]

	@ Save arguments.  We save r4 to realign the stack and to hold
	@ the hwcap value used to decide whether to save VFP registers.
	push	{r0-r4}
	cfi_adjust_cfa_offset (20)
	cfi_rel_offset (r0, 0)
	cfi_rel_offset (r1, 4)
	cfi_rel_offset (r2, 8)
	cfi_rel_offset (r3, 12)
	cfi_rel_offset (r4, 16)

#ifdef SHARED
	@ Preserve all call-clobbered VFP registers across _dl_fixup.
	@ VFP may be used by IFUNC resolvers, audit modules, interposed
	@ malloc, and the __aeabi_mem* helpers required by RTABI32,
	@ which mandates that those helpers only corrupt integer core
	@ registers.
	LDR_GLOBAL (r4, r3, C_SYMBOL_NAME(_rtld_global_ro), \
		    RTLD_GLOBAL_RO_DL_HWCAP_OFFSET)

	tst	r4, #HWCAP_ARM_VFP
	beq	.Lno_vfp_save

# define VFP_STACK_REQ (32*8 + 8)
	sub	sp, sp, VFP_STACK_REQ
	cfi_adjust_cfa_offset (VFP_STACK_REQ)
	mov	r3, sp
	.inst	0xeca30b20	@ vstmia r3!, {d0-d15}
	tst	r4, #HWCAP_ARM_VFPD32
	beq	1f
	.inst	0xece30b20	@ vstmia r3!, {d16-d31}
1:	.inst	0xeef12a10	@ vmrs r2, fpscr
	str	r2, [r3]
.Lno_vfp_save:
#endif /* SHARED */

	@ get pointer to linker struct
	ldr	r0, [lr, #-4]

	@ prepare to call _dl_fixup()
	@ change &GOT[n+3] into 8*n        NOTE: reloc are 8 bytes each
	sub	r1, ip, lr
	sub	r1, r1, #4
	add	r1, r1, r1

	@ call fixup routine
	bl	_dl_fixup

	@ save the return
	mov	ip, r0

#ifdef SHARED
	tst	r4, #HWCAP_ARM_VFP
	beq	.Lno_vfp_restore
	mov	r3, sp
	.inst	0xecb30b20	@ vldmia r3!, {d0-d15}
	tst	r4, #HWCAP_ARM_VFPD32
	beq	2f
	.inst	0xecf30b20	@ vldmia r3!, {d16-d31}
2:	ldr	r2, [r3]
	.inst	0xeee12a10	@ vmsr fpscr, r2
	add	sp, sp, VFP_STACK_REQ
	cfi_adjust_cfa_offset (-VFP_STACK_REQ)
.Lno_vfp_restore:
#endif /* SHARED */

	@ get arguments and return address back.  We restore r4 to
	@ its original value as well.
	pop	{r0-r4,lr}
	cfi_adjust_cfa_offset (-24)

	@ jump to the newly found address
	BX(ip)

	cfi_endproc
	.size _dl_runtime_resolve, .-_dl_runtime_resolve

#if !defined PROF && defined SHARED
	.globl _dl_runtime_profile
	.type _dl_runtime_profile, #function
	CFI_SECTIONS
	cfi_startproc
	.align 2
_dl_runtime_profile:
	cfi_adjust_cfa_offset (4)
	cfi_rel_offset (lr, 0)

	@ we get called with
	@ 	stack[0] contains the return address from this call
	@	ip contains &GOT[n+3] (pointer to function)
	@	lr points to &GOT[2]

	@ Stack layout:
	@ 212 - saved lr
	@ 208 - framesize returned from pltenter
	@ 16 - La_arm_regs
	@ 8 - Saved two arguments to _dl_profile_fixup
	@ 4 - Saved result of _dl_profile_fixup
	@ 0 - outgoing argument to _dl_profile_fixup
	@ For now, we only save the general purpose registers.

	sub	sp, sp, #196
	cfi_adjust_cfa_offset (196)
	stmia	sp, {r0-r3}
	cfi_rel_offset (r0, 0)
	cfi_rel_offset (r1, 4)
	cfi_rel_offset (r2, 8)
	cfi_rel_offset (r3, 12)

	sub	sp, sp, #16
	cfi_adjust_cfa_offset (16)

	@ Save sp and lr.
	add	r0, sp, #216
	str	r0, [sp, #32]
	ldr	r2, [sp, #212]
	str	r2, [sp, #36]

	@ get pointer to linker struct
	ldr	r0, [lr, #-4]

	@ prepare to call _dl_profile_fixup()
	@ change &GOT[n+3] into 8*n        NOTE: reloc are 8 bytes each
	sub	r1, ip, lr
	sub	r1, r1, #4
	add	r1, r1, r1

	@ Save these two arguments for pltexit.
	add	r3, sp, #8
	stmia	r3!, {r0,r1}

	@ Preserve all call-clobbered VFP registers across
	@ _dl_profile_fixup.  See the matching comment in
	@ _dl_runtime_resolve above for the rationale (BZ 34144,
	@ BZ 15792).
	@
	@ Stack layout below the current sp (which becomes the new sp
	@ after the sub):
	@   sp +  0 ..   3: outgoing arg (framesizep) for _dl_profile_fixup
	@   sp +  4 ..   7: saved hwcap (so we can test it after the call)
	@   sp +  8 ..  11: saved r2 (used as scratch for LDR_GLOBAL)
	@   sp + 12 ..  15: padding (for 8-byte alignment of the VFP area)
	@   sp + 16 .. 16+VFP_STACK_REQ-1: VFP regs + fpscr
#define VFP_PROFILE_STACK (16 + VFP_STACK_REQ)
	sub	sp, sp, #VFP_PROFILE_STACK
	cfi_adjust_cfa_offset (VFP_PROFILE_STACK)

	@ r2 holds the retaddr (3rd arg to _dl_profile_fixup); spill
	@ it so we can use it as the LDR_GLOBAL destination.
	str	r2, [sp, #8]

	LDR_GLOBAL (r2, ip, C_SYMBOL_NAME(_rtld_global_ro), \
		    RTLD_GLOBAL_RO_DL_HWCAP_OFFSET)
	str	r2, [sp, #4]

	tst	r2, #HWCAP_ARM_VFP
	beq	.Lprofile_no_vfp_save
	add	ip, sp, #16
	.inst	0xecac0b20	@ vstmia ip!, {d0-d15}
	tst	r2, #HWCAP_ARM_VFPD32
	beq	7f
	.inst	0xecec0b20	@ vstmia ip!, {d16-d31}
7:	.inst	0xeef12a10	@ vmrs r2, fpscr
	str	r2, [ip]
.Lprofile_no_vfp_save:

	@ Restore r2 (retaddr) for _dl_profile_fixup.
	ldr	r2, [sp, #8]

	@ Set up extra args for _dl_profile_fixup.
	@ The framesize slot is at the old sp+208, which is the new
	@ sp + VFP_PROFILE_STACK + 208 -- compute in two steps because
	@ the combined offset is not encodable as an ARM immediate.
	add	ip, sp, #VFP_PROFILE_STACK
	add	ip, ip, #208
	str	ip, [sp, #0]

	@ call profiling fixup routine
	bl	_dl_profile_fixup

	@ Restore VFP registers.  r0 holds the resolved function
	@ address; r1/r2/ip are caller-saved by the call.
	ldr	r1, [sp, #4]
	tst	r1, #HWCAP_ARM_VFP
	beq	.Lprofile_no_vfp_restore
	add	ip, sp, #16
	.inst	0xecbc0b20	@ vldmia ip!, {d0-d15}
	tst	r1, #HWCAP_ARM_VFPD32
	beq	8f
	.inst	0xecfc0b20	@ vldmia ip!, {d16-d31}
8:	ldr	r2, [ip]
	.inst	0xeee12a10	@ vmsr fpscr, r2
.Lprofile_no_vfp_restore:
	add	sp, sp, #VFP_PROFILE_STACK
	cfi_adjust_cfa_offset (-VFP_PROFILE_STACK)

	@ The address to call is now in r0.

	@ Check whether we're wrapping this function.
	ldr	ip, [sp, #208]
	cmp	ip, #0
	bge	1f
	cfi_remember_state

	@ save the return
	mov	ip, r0

	@ get arguments and return address back
	add	sp, sp, #16
	cfi_adjust_cfa_offset (-16)
	ldmia	sp, {r0-r3,sp,lr}
	cfi_adjust_cfa_offset (-200)

	@ jump to the newly found address
	BX(ip)

	cfi_restore_state
1:
	@ The new frame size is in ip.

	@ New stack layout:
	@ 268 - saved r7
	@ 264 - saved result of _dl_profile_fixup
	@ 72 - La_arm_regs
	@ 64 - Saved two arguments to _dl_profile_fixup
	@ 0 - La_arm_retval
	@ For now, we only save the general purpose registers.

	@ Build the new frame.
	str	r7, [sp, #212]
	cfi_rel_offset (r7, 212)
	sub	r7, sp, #56
	cfi_def_cfa_register (r7)
	cfi_adjust_cfa_offset (56)
	sub	sp, sp, ip
	bic	sp, sp, #7

	@ Save the _dl_profile_fixup result around the call to memcpy.
	str	r0, [r7, #264]

	@ Copy the stack arguments.
	mov	r0, sp
	add	r1, r7, #272
	mov	r2, ip
	bl	memcpy

	@ Call the function.
	add	ip, r7, #72
	ldmia	ip, {r0-r3}
	ldr	ip, [r7, #264]
	BLX(ip)
	stmia	r7, {r0-r3}

	@ Call pltexit.
	add	ip, r7, #64
	ldmia	ip, {r0,r1}
	add	r2, r7, #72
	add	r3, r7, #0
	bl	_dl_audit_pltexit

	@ Return to caller.
	ldmia	r7, {r0-r3}
	mov	sp, r7
	cfi_def_cfa_register (sp)
	ldr	r7, [sp, #268]
	ldr	lr, [sp, #92]
	add	sp, sp, #272
	cfi_adjust_cfa_offset (-272)
	BX(lr)

	cfi_endproc
	.size _dl_runtime_profile, .-_dl_runtime_profile
#endif
	.previous
