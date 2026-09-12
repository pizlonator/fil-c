/* ld.so _start code.
   Copyright (C) 2022-2026 Free Software Foundation, Inc.

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

#include <sysdep.h>
#include <rtld-global-offsets.h>

ENTRY (_start)
	/* Create an initial frame with 0 LR and FP */
	cfi_undefined (x30)
	mov	x29, #0
	mov	x30, #0

	/* Load and relocate all library dependencies.  */
	mov	x0, sp
	bl	_dl_start
	/* Returns user entry point in x0.  */
	mov	x21, x0

	/* Use GL(dl_aarch64_gcs) to set the shadow stack status.  */
	adrp	x16, _rtld_local
	add	x16, x16, :lo12:_rtld_local
	ldr	x22, [x16, GL_DL_AARCH64_GCS_OFFSET]
	cbz	x22, L(skip_gcs_enable)

	/* Enable GCS before user code runs.  Note that IFUNC resolvers and
	   LD_AUDIT hooks may run before, but should not create threads.  */
#define PR_SET_SHADOW_STACK_STATUS  75
#define PR_LOCK_SHADOW_STACK_STATUS 76
#define PR_SHADOW_STACK_ENABLE      (1UL << 0)
	mov	x0, PR_SET_SHADOW_STACK_STATUS
	mov	x1, PR_SHADOW_STACK_ENABLE
	mov	x2, 0
	mov	x3, 0
	mov	x4, 0
	mov	x8, #SYS_ify(prctl)
	svc	0x0
	cbnz	w0, L(failed_gcs_enable)
	/* Check if we need to lock GCS features.  */
	/* If the aarch64_gcs tunable is either 0 or 2 do not lock GCS.  */
	tst	x22, #-3
	beq	L(skip_gcs_enable)
	mov	x0, PR_LOCK_SHADOW_STACK_STATUS
	/* Lock everything including future operations.  */
	mov	x1, ~0
	mov	x2, 0
	mov	x3, 0
	mov	x4, 0
	mov	x8, #SYS_ify(prctl)
	svc	0x0
	cbnz	w0, L(failed_gcs_lock)
L(skip_gcs_enable):

.globl _dl_start_user
.type _dl_start_user, %function
_dl_start_user:
	/* Get argc.  */
	ldr	x1, [sp]
	/* Get argv.  */
	add	x2, sp, 8
	/* Compute envp.  */
	add	x3, x2, x1, lsl 3
	add	x3, x3, 8
	/* Run the init functions of the loaded modules.  */
	ldr	x0, [x16]
	bl	_dl_init
	/* Load the finalizer function.  */
	adrp	x0, _dl_fini
	add	x0, x0, :lo12:_dl_fini
	/* Jump to the user's entry point.  */
	mov	x16, x21
	br	x16

L(failed_gcs_enable):
	b	_dl_gcs_enable_failed

L(failed_gcs_lock):
	b	_dl_gcs_lock_failed

END (_start)
