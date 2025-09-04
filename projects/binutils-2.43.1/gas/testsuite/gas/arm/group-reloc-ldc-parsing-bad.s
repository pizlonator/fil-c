@ Tests for LDC group relocations that are meant to fail during parsing.

	.macro ldctest insn reg

	\insn	0, \reg, [r0, #:pc_g0_nc:(sym)]
	\insn	0, \reg, [r0, #:pc_g1_nc:(sym)]
	\insn	0, \reg, [r0, #:sb_g0_nc:(sym)]
	\insn	0, \reg, [r0, #:sb_g1_nc:(sym)]

	\insn	0, \reg, [r0, #:foo:(sym)]

	.endm

	.macro ldctest2 insn reg

	\insn	\reg, [r0, #:pc_g0_nc:(sym)]
	\insn	\reg, [r0, #:pc_g1_nc:(sym)]
	\insn	\reg, [r0, #:sb_g0_nc:(sym)]
	\insn	\reg, [r0, #:sb_g1_nc:(sym)]

	\insn	\reg, [r0, #:foo:(sym)]

	.endm

	ldctest ldc c0
	ldctest ldcl c0
	ldctest ldc2 c0
	ldctest ldc2l c0

	ldctest stc c0
	ldctest stcl c0
	ldctest stc2 c0
	ldctest stc2l c0












	.fpu	vfp

	ldctest2 flds s0
	ldctest2 fsts s0

	ldctest2 fldd d0
	ldctest2 fstd d0

	ldctest2 vldr d0		FIXME
	ldctest2 vstr d0

