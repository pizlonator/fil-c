# Testcase for a variety of ld st instructions.
# stg ops must generate the sp update arithmetic insn, when applicable
# due to writeback.
	.text
	.align  2
	.global foo
	.type   foo, %function
foo:
# ldstpair_indexed
	stp     wzr, wzr, [sp, 48]!
	stp     x0, x1, [sp, 32]!
	stp     d8, d9, [sp, -64]!
	ldp     d8, d9, [sp], 64
# 32-bit FP regs
	stp     s5, s6, [sp, -96]!
	ldp     s5, s6, [sp], 96
# 32-bit INT regs
	stp     w1, w2, [sp, -128]!
	ldp     w1, w2, [sp], 128
# ldstpair_off
	stp     q0, q1, [sp, 64]
	stp     wzr, wzr, [sp, 48]
	ldp     q31, q30, [x0]
        stp     xzr, x19, [sp, 16]
        stp     x19, xzr, [sp, 16]
# ldst_imm9
	ldrb    w7, [sp, 248]!
	ldr     q29, [sp, 48]!
	ldr     wzr, [sp, 32]!
	ldr     x3, [sp], 32
# 32-bit ldr
	ldr     s1, [sp], 64
# ldst_pos
	ldr     wzr, [sp, 48]
	str     x29, [sp, 32]
	ldr     x29, [sp, 32]
# store tag
	stg     sp, [sp, 32]!
# store tag pair
	stgp    x27, x28, [sp, 48]!
# ldpsw / ldrsw
	ldpsw   x19, x20, [sp, -256]!
	ldrsw   x21, [sp, -128]!
# ldrsb / ldrsh
	ldrsb   x23, [sp, -112]!
	ldrsh   x24, [sp, -104]!
# ldrb / ldrh / strb / strh
	ldrb    w25, [sp, -96]!
	ldrh    w26, [sp, -88]!
	strb    w25, [sp, -96]!
	strh    w26, [sp, -88]!
	ret
	.size   foo, .-foo
