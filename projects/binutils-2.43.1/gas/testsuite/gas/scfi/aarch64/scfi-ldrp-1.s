# Testcase for various ldp / ldr instructions.
# This test also serves for checking callee-saved regs.
	.text
	.align  2
	.global foo
	.type   foo, %function
foo:
	.cfi_startproc
	stp     x29, x30, [sp, -128]!
	.cfi_def_cfa_offset 128
	.cfi_offset 29, -128
	.cfi_offset 30, -120
	mov     x29, sp
	.cfi_def_cfa_register 29
	stp     x19, x20, [sp, 16]
	.cfi_offset 19, -112
	.cfi_offset 20, -104
	stp     x21, x22, [sp, 32]
	.cfi_offset 21, -96
	.cfi_offset 22, -88
	stp     x23, x24, [sp, 48]
	.cfi_offset 23, -80
	.cfi_offset 24, -72
	stp     x25, x26, [sp, 64]
	.cfi_offset 25, -64
	.cfi_offset 26, -56
	str     x27, [sp, 80]
	.cfi_offset 27, -48
	mov     w0, 0
	mov     sp, x29
	.cfi_def_cfa_register 31
	ldp     x19, x20, [sp, 16]
	.cfi_restore 19
	.cfi_restore 20
	ldp     x21, x22, [sp, 32]
	.cfi_restore 21
	.cfi_restore 22
	ldp     x23, x24, [sp, 48]
	.cfi_restore 23
	.cfi_restore 24
	ldp     x25, x26, [sp, 64]
	.cfi_restore 25
	.cfi_restore 26
	ldr     x27, [sp, 80]
	.cfi_restore 27
	ldp     x29, x30, [sp], 128
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
