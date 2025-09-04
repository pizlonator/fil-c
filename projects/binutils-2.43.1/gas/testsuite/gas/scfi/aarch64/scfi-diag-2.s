# Testcase that triggers a diagnostic for unreachable code for
# the block of code after label .L2.  This also serves as a testcase
# for cfg creation, as the said code block should not have been
# included in the CFG (and hence the warning).
	.type   foo, %function
foo:
	.cfi_startproc
	ldr     w1, [x0]
	cmp     w1, 8
	stp     x19, x20, [sp, 16]
	.cfi_offset 19, 16
	.cfi_offset 20, 24
	b       .L1
.L2:
	mov     w0, w1
	ret
.L1:
	mov     w0, w1
	ldp     x19, x20, [sp, 16]
	.cfi_restore 19
	.cfi_restore 20
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
