/* Libc internal support routine for SME.
   Copyright (C) 2023 Free Software Foundation, Inc.

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
   <http://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <rtld-global-offsets.h>

#define HWCAP2_SME_BIT 23

/* Disable ZA.  Call ABI:
   - Private ZA, streaming-compatible.
   - x0-x13, x19-x29, sp and fp regs are call preserved.
   - On return tpidr2_el0 = 0, ZA = 0.
   - Takes no argument.
   - Does not return a value.
   - Can abort on failure (then registers are not preserved).  */

ENTRY (__libc_arm_za_disable)

	/* Check if SME is available.  */
#ifdef SHARED
	/* In libc.so.  */
	adrp	x14, :got:_rtld_global_ro
	ldr	x14, [x14, :got_lo12:_rtld_global_ro]
	ldr	x14, [x14, GLRO_DL_HWCAP2_OFFSET]
#else
	/* In libc.a, may be PIC.  */
	adrp	x14, _dl_hwcap2
	ldr	x14, [x14, :lo12:_dl_hwcap2]
#endif
	tbz	x14, HWCAP2_SME_BIT, L(end)

	.inst	0xd53bd0ae  /* mrs	x14, tpidr2_el0  */
	cbz	x14, L(end)

	/* Check reserved bytes, abort on unknown extensions.  */
	ldrh	w15, [x14, 10]
	ldr	w16, [x14, 12]
	orr	w15, w15, w16
	cbnz	w15, L(fail)

	ldr	x16, [x14]
	cbz	x16, L(end)
	ldrh	w17, [x14, 8]
	cbz	w17, L(end)

	/* x14: tpidr2, x15: 0,
	   x16: za_save_buffer, x17: num_za_save_slices.  */

L(save_loop):
	.inst	0xe1206200  /* str	za[w15, 0], [x16]  */
	.inst	0xe1206201  /* str	za[w15, 1], [x16, 1, mul vl] */
	.inst	0xe1206202  /* str	za[w15, 2], [x16, 2, mul vl] */
	.inst	0xe1206203  /* str	za[w15, 3], [x16, 3, mul vl] */
	.inst	0xe1206204  /* str	za[w15, 4], [x16, 4, mul vl] */
	.inst	0xe1206205  /* str	za[w15, 5], [x16, 5, mul vl] */
	.inst	0xe1206206  /* str	za[w15, 6], [x16, 6, mul vl] */
	.inst	0xe1206207  /* str	za[w15, 7], [x16, 7, mul vl] */
	.inst	0xe1206208  /* str	za[w15, 8], [x16, 8, mul vl] */
	.inst	0xe1206209  /* str	za[w15, 9], [x16, 9, mul vl] */
	.inst	0xe120620a  /* str	za[w15, 10], [x16, 10, mul vl] */
	.inst	0xe120620b  /* str	za[w15, 11], [x16, 11, mul vl] */
	.inst	0xe120620c  /* str	za[w15, 12], [x16, 12, mul vl] */
	.inst	0xe120620d  /* str	za[w15, 13], [x16, 13, mul vl] */
	.inst	0xe120620e  /* str	za[w15, 14], [x16, 14, mul vl] */
	.inst	0xe120620f  /* str	za[w15, 15], [x16, 15, mul vl] */
	add	w15, w15, 16
	.inst	0x04305a10  /* addsvl	x16, x16, 16  */
	cmp	w17, w15
	bhi	L(save_loop)
	.inst	0xd51bd0bf  /* msr	tpidr2_el0, xzr  */
	.inst	0xd503447f  /* smstop	za  */
L(end):
	ret
L(fail):
#if HAVE_AARCH64_PAC_RET
	PACIASP
	cfi_window_save
#endif
	stp	x29, x30, [sp, -32]!
	cfi_adjust_cfa_offset (32)
	cfi_rel_offset (x29, 0)
	cfi_rel_offset (x30, 8)
	mov	x29, sp
	.inst	0x04e0e3f0  /* cntd	x16  */
	str	x16, [sp, 16]
	cfi_rel_offset (46, 16)
	.inst	0xd503467f  /* smstop  */
	adrp	x0, L(msg)
	add	x0, x0, :lo12:L(msg)
	bl	HIDDEN_JUMPTARGET (__libc_fatal)
END (__libc_arm_za_disable)

	.section        .rodata
	.align  3
L(msg):
	.string "FATAL: __libc_arm_za_disable failed.\n"
