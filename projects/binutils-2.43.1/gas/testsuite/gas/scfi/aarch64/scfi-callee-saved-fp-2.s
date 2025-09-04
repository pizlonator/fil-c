# Testcase for callee-saved FP registers.
# Use Q registers
	.text
	.align  2
	.global foo
	.type   foo, %function
foo:
	.cfi_startproc
	stp     x29, x30, [sp, -256]!
	.cfi_def_cfa_offset 256
	.cfi_offset 29, -256
	.cfi_offset 30, -248
	mov     x29, sp
	.cfi_def_cfa_register 29
	str     q8, [sp, 16]
	.cfi_offset 72, -232
	str     q9, [sp, 32]
	.cfi_offset 73, -216
	stp     q10, q11, [sp, 64]
	.cfi_offset 74, -184
	.cfi_offset 75, -168
	stp     q12, q13, [sp, 96]
	.cfi_offset 76, -152
	.cfi_offset 77, -136
	stp     q14, q15, [sp, 128]
	.cfi_offset 78, -120
	.cfi_offset 79, -104
	mov     w0, 0
	mov     sp, x29
	.cfi_def_cfa_register 31
	ldr     q8, [sp, 16]
	.cfi_restore 72
	ldr     q9, [sp, 32]
	.cfi_restore 73
	ldp     q10, q11, [sp, 64]
	.cfi_restore 74
	.cfi_restore 75
	ldp     q12, q13, [sp, 96]
	.cfi_restore 76
	.cfi_restore 77
	ldp     q14, q15, [sp, 128]
	.cfi_restore 78
	.cfi_restore 79
	ldp     x29, x30, [sp], 256
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
