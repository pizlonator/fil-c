# Testcase for callee-saved FP registers.
# Uses a combination of str/stp and ldr/ldp
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
	str     d8, [sp, 16]
	.cfi_offset 72, -112
	str     d9, [sp, 24]
	.cfi_offset 73, -104
	stp     d10, d11, [sp, 32]
	.cfi_offset 74, -96
	.cfi_offset 75, -88
	stp     d12, d13, [sp, 48]
	.cfi_offset 76, -80
	.cfi_offset 77, -72
	stp     d14, d15, [sp, 64]
	.cfi_offset 78, -64
	.cfi_offset 79, -56
	mov     w0, 0
	mov     sp, x29
	.cfi_def_cfa_register 31
	ldr     d8, [sp, 16]
	.cfi_restore 72
	ldr     d9, [sp, 24]
	.cfi_restore 73
	ldp     d10, d11, [sp, 32]
	.cfi_restore 74
	.cfi_restore 75
	ldp     d12, d13, [sp, 48]
	.cfi_restore 76
	.cfi_restore 77
	ldp     d14, d15, [sp, 64]
	.cfi_restore 78
	.cfi_restore 79
	ldp     x29, x30, [sp], 128
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
