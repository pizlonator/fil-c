# Testcase where immediate used for stack allocation is a wide
# one.  Since SCFI does not currently have any data-flow
# capabilities, this is currently not supported.
	.global foo
	.type   foo, %function
foo:
	.cfi_startproc
	mov     x16, 4384
	sub     sp, sp, x16
	.cfi_def_cfa_offset 4384
	stp     x29, x30, [sp]
	.cfi_offset 29, -4384
	.cfi_offset 30, -4376
	mov     x29, sp
	str     x0, [sp, 24]
	str     x1, [sp, 16]
	add     x0, sp, 4096
	add     x0, x0, 112
	bl      bar
.L1:
	str     xzr, [sp, 4376]
.L2:
	ldp     x29, x30, [sp]
	mov     x16, 4384
	add     sp, sp, x16
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
