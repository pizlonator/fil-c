# Testcase for forward flow of SCFI information.
# This testcase has two paths landing at the exit basic block,
# where only one of the exit paths has a save/restore of x19
# (while the other does not).
	.type   foo, %function
foo:
	.cfi_startproc
	stp     x29, x30, [sp, -32]!
	.cfi_def_cfa_offset 32
	.cfi_offset 29, -32
	.cfi_offset 30, -24
	mov     x29, sp
	.cfi_def_cfa_register 29
# do some work ...
	cbz     x0, .L2
	str     x19, [sp, 16]
	.cfi_offset 19, -16
# do some other work ...
	cbz     w0, .L3
	ldr     x19, [sp, 16]
	.cfi_restore 19
.L2:
	mov     w0, 1
.L1:
	ldp     x29, x30, [sp], 32
	.cfi_def_cfa_register 31
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	ret
.L3:
	.cfi_def_cfa_offset 32
	.cfi_offset 19, -16
	.cfi_offset 29, -32
	.cfi_offset 30, -24
# do yet some other work before return
	ldr     x19, [sp, 16]
	.cfi_restore 19
	b       .L1
	.cfi_endproc
	.size    foo, .-foo
