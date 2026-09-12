/* Thread-local storage handling in the ELF dynamic linker.  ARM version.
   Copyright (C) 2006-2026 Free Software Foundation, Inc.
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

#include <sysdep.h>
#include <arm-features.h>
#include <tls.h>
#include <rtld-global-offsets.h>
#include "tlsdesc.h"

	.text
	@ emit debug information with cfi
	@ use arm-specific pseudos for unwinding itself
	CFI_SECTIONS
	.hidden _dl_tlsdesc_return
	.global	_dl_tlsdesc_return
	.type	_dl_tlsdesc_return,#function
	cfi_startproc
	eabi_fnstart
	.align 2
_dl_tlsdesc_return:
	ldr	r0, [r0]
	BX	(lr)
	eabi_fnend
	cfi_endproc
	.size	_dl_tlsdesc_return, .-_dl_tlsdesc_return

	.hidden _dl_tlsdesc_undefweak
	.global	_dl_tlsdesc_undefweak
	.type	_dl_tlsdesc_undefweak,#function
	cfi_startproc
	eabi_fnstart
	.align 2
_dl_tlsdesc_undefweak:
	GET_TLS (r1)
	rsb 	r0, r0, #0
	BX	(lr)
	cfi_endproc
	eabi_fnend
	.size	_dl_tlsdesc_undefweak, .-_dl_tlsdesc_undefweak

#ifdef SHARED
	.hidden _dl_tlsdesc_dynamic
	.global	_dl_tlsdesc_dynamic
	.type	_dl_tlsdesc_dynamic,#function


/*
	The assembly code that follows is a rendition of the following
	C code, hand-optimized a little bit.

ptrdiff_t
_dl_tlsdesc_dynamic(struct tlsdesc *tdp)
{
       struct tlsdesc_dynamic_arg *td = tdp->argument.pointer;
       dtv_t *dtv = (dtv_t *)THREAD_DTV();
       if (__builtin_expect (td->gen_count <= dtv[0].counter
                             && dtv[td->tlsinfo.ti_module].pointer.val
                                != TLS_DTV_UNALLOCATED,
                             1))
               return dtv[td->tlsinfo.ti_module].pointer.val +
                       td->tlsinfo.ti_offset - __builtin_thread_pointer();

       return __tls_get_addr (&td->tlsinfo) - __builtin_thread_pointer();
}

*/
	cfi_startproc
	eabi_fnstart
	.align 2
_dl_tlsdesc_dynamic:
	/* Our calling convention is to clobber r0, r1 and the processor
	   flags.  All others that are modified must be saved.  r5 is
	   used as the hwcap value to avoid reload after __tls_get_addr
	   call.  If required we will save the vector register on the slow
	   path.  */
	eabi_save ({r2,r3,r4,r5,ip,lr})
	push	{r2,r3,r4,r5,ip,lr}
	cfi_adjust_cfa_offset (24)
	cfi_rel_offset (r2,0)
	cfi_rel_offset (r3,4)
	cfi_rel_offset (r4,8)
	cfi_rel_offset (r5,12)
	cfi_rel_offset (ip,16)
	cfi_rel_offset (lr,20)

	ldr	r1, [r0] /* td */
	GET_TLS (lr)
	mov	r4, r0 /* r4 = tp */
	ldr	r0, [r0]
	ldr	r2, [r1, #8] /* gen_count */
	ldr	r3, [r0]
	cmp	r2, r3
	bhi	1f
	ldr	r3, [r1]
#ifndef ARM_NO_INDEX_REGISTER
	ldr	r2, [r0, r3, lsl #3]
#else
	add	lr, r0, r3, lsl #3
	ldr	r2, [lr]
#endif
	cmn	r2, #1
	ittt	ne
	ldrne	r3, [r1, #4]
	addne	r3, r2, r3
	rsbne	r0, r4, r3
	bne	2f
1:	mov	r0, r1

	/* Load the hwcap to check for vector support.  */
	ldr     r2, 3f
	ldr     r1, .Lrtld_global_ro
0:	add     r2, pc, r2
	ldr     r2, [r2, r1]
	ldr     r5, [r2, #RTLD_GLOBAL_RO_DL_HWCAP_OFFSET]

#ifdef __SOFTFP__
	tst     r5, #HWCAP_ARM_VFP
	beq     .Lno_vfp
#endif

	/* Store the VFP registers.  Don't use VFP instructions directly
	   because this code is used in non-VFP multilibs.  */
#define VFP_STACK_REQ (32*8 + 8)
	sub	sp, sp, VFP_STACK_REQ
	cfi_adjust_cfa_offset (VFP_STACK_REQ)
	mov	r3, sp
	.inst	0xeca30b20	/* vstmia r3!, {d0-d15} */
	tst	r5, #HWCAP_ARM_VFPD32
	beq	4f
	.inst	0xece30b20	/* vstmia r3!, {d16-d31}  */
	/* Store the floating-point status register.  */
4:	.inst	0xeef12a10	/* vmrs	r2, fpscr */
	str	r2, [r3]
.Lno_vfp:
	bl	HIDDEN_JUMPTARGET(__tls_get_addr)
	rsb	r0, r4, r0
#ifdef __SOFTFP__
	tst     r5, #HWCAP_ARM_VFP
	beq     2f
#endif
	mov	r3, sp
	.inst	0xecb30b20	/* vldmia r3!, {d0-d15}  */
	tst	r5, #HWCAP_ARM_VFPD32
	beq	5f
	.inst	0xecf30b20	/* vldmia r3!, {d16-d31}  */
	ldr	r4, [r3]
5:	.inst	0xeee14a10	/* vmsr	fpscr, r4  */
	add	sp, sp, VFP_STACK_REQ
	cfi_adjust_cfa_offset (-VFP_STACK_REQ)

2:
#if ((defined (__ARM_ARCH_4T__) && defined (__THUMB_INTERWORK__)) \
     || defined (ARM_ALWAYS_BX))
	pop	{r2,r3,r4,r5,ip, lr}
	cfi_adjust_cfa_offset (-20)
	cfi_restore (lr)
	cfi_restore (ip)
	cfi_restore (r5)
	cfi_restore (r4)
	cfi_restore (r3)
	cfi_restore (r2)
	bx	lr
#else
	pop	{r2,r3,r4,r5,ip, pc}
#endif
	eabi_fnend
	cfi_endproc
	.size	_dl_tlsdesc_dynamic, .-_dl_tlsdesc_dynamic
	.hidden HIDDEN_JUMPTARGET(__tls_get_addr)

3:      .long   _GLOBAL_OFFSET_TABLE_ - 0b - PC_OFS
.Lrtld_global_ro:
	.long   C_SYMBOL_NAME(_rtld_global_ro)(GOT)
#endif /* SHARED */
