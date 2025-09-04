# Testcase for backward flow of SCFI state.
# The cfg has two exit paths, with epilogue duplicated in
# the two.
#
# SCFI must synthesize the remember_state / restore_state pair.
# Note how SCFI does not necessary generate the least number of
# CFI directives (.cfi_remember_state can possibly be clubbed
# together with other immediately following CFI directives).
# This is not a correctness issue, however.
	.global foo
	.type   foo, %function
foo:
	.cfi_startproc
	stp     x29, x30, [sp, -48]!
	.cfi_def_cfa_offset 48
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	mov     x29, sp
	.cfi_def_cfa_register 29
	cmp     w4, w19
	bge     .L1

.L2:
	.cfi_remember_state
	bl      bar
	ldp     x29, x30, [sp], 48
	.cfi_def_cfa_register 31
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
.L1:
	.cfi_restore_state
	cbz     w3, .L2
	ldp     x29, x30, [sp], 48
	.cfi_def_cfa_register 31
	.cfi_restore 29
	.cfi_restore 30
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
	.size   foo, .-foo
