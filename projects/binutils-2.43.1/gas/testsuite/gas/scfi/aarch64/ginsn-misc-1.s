# Testcase for a variety of misc instructions.
# Ensure graceful handling, irrespective of ginsn generation.
# - ldrsw, ldpsw ops need a mention: they are 32-bit loads from memory
#   and cannot be used for reg restore.  For CFI purposes, the width has to be
#   8 bytes or more. Expect no memory ginsn ever as ginsn do not track width
#   of memory accesses yet.
symbol:
	.string "w"

	.type   foo, %function
foo:
	adrp    x0, symbol
	add     sp, x0, :lo12:symbol
	movk    x0, 0x1, lsl 16
	prfm    PLDL1STRM, [sp]
	irg     x0, sp
	stg     x0, [x0]
	stgp    x21, x22, [sp, -112]
	addg    x0, sp, #0x3f0, #0xf
	addg    sp, x0, #0x2a0, #0xf
	ldpsw   x20, x21, [sp, -128]
        ldpsw   x19, x20, [sp]
        ldrsw   x27, [x4]
	ldrsb   x20, [sp]
	ldrsh   x20, [sp]
# Test a few insn with wzr / xzr
        mov     xzr, 33
        mov     x0, xzr
        add     xzr, x0, x1
        add     sp, x1, xzr
	add     xzr, xzr, xzr
	ret
	.size   foo,.-foo
