## Testcase with a variety of str/stp instructions
	.text
	.globl  foo
	.type   foo, @function
foo:
	.cfi_startproc
# Pre-indexed addressing is like offset addressing, except that
# the base pointer is updated as a result of the instruction.
	stp     x29, x30, [sp, -128]!
	.cfi_def_cfa_offset 128
	.cfi_offset 29, -128
	.cfi_offset 30, -120
	mov     x29, sp
	.cfi_def_cfa_register 29
# Offset addressing mode is when ann offset can be applied optionally to the
# base address.
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
# Stores non callee-saved register on stack.
	str     w0, [x29, 124]
	str     wzr, [x29, 120]
	str     w0, [x29, 120]
	ret
	.cfi_endproc
	.size   foo, .-foo
