# Testcase for various ldp / ldr instructions
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
	mov     sp, x29
	.cfi_def_cfa_register 31
# Post-indexed ldr
	ldr     x29, [sp], 8
	.cfi_restore 29
	.cfi_def_cfa_offset 120
# Post-indexed ldr
	ldr     x30, [sp], 120
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
