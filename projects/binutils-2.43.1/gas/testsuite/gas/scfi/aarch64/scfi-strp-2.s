# Testcase for a variety of stp/str including a post-indexed store
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
# post-indexed store, a stack corrupting one which over-writes
# x29!  Only for testing purposes for now
# This does not generate a .cfi_def_cfa_offset 208 because
# CFA is REG_FP based
	str     x27, [sp], 80
	.cfi_offset 27, -128
	mov     sp, x29
	.cfi_def_cfa_register 31
	ldr     x29, [sp], 8
	.cfi_restore 29
	.cfi_def_cfa_offset 120
	ldr     x30, [sp], 120
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
