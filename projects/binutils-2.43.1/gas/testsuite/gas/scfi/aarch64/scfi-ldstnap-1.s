# Testcase for various ldnp / stnp instructions
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
	stp     x19, x20, [sp, 16]
	.cfi_offset 19, -112
	.cfi_offset 20, -104
	ldp     x19, x20, [sp, 16]
	.cfi_restore 19
	.cfi_restore 20
	ldr     x29, [sp], 8
	.cfi_restore 29
	.cfi_def_cfa_offset 120
	ldr     x30, [sp], 120
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
