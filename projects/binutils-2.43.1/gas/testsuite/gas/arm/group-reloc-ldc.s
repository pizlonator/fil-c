@ LDC group relocation tests.

	.text

@ LDC/LDCL/LDC2/LDC2L/STC/STCL/STC2/STC2L

	.macro ldctest load store

	\load	0, c0, [r0, #:pc_g0:(f + 0x214)]
	\load	0, c0, [r0, #:pc_g1:(f + 0x214)]
	\load	0, c0, [r0, #:pc_g2:(f + 0x214)]

	\load	0, c0, [r0, #:sb_g0:(f + 0x214)]
	\load	0, c0, [r0, #:sb_g1:(f + 0x214)]
	\load	0, c0, [r0, #:sb_g2:(f + 0x214)]

	\store	0, c0, [r0, #:pc_g0:(f + 0x214)]
	\store	0, c0, [r0, #:pc_g1:(f + 0x214)]
	\store	0, c0, [r0, #:pc_g2:(f + 0x214)]

	\store	0, c0, [r0, #:sb_g0:(f + 0x214)]
	\store	0, c0, [r0, #:sb_g1:(f + 0x214)]
	\store	0, c0, [r0, #:sb_g2:(f + 0x214)]

	\load	0, c0, [r0, #:pc_g0:(f - 0x214)]
	\load	0, c0, [r0, #:pc_g1:(f - 0x214)]
	\load	0, c0, [r0, #:pc_g2:(f - 0x214)]

	\load	0, c0, [r0, #:sb_g0:(f - 0x214)]
	\load	0, c0, [r0, #:sb_g1:(f - 0x214)]
	\load	0, c0, [r0, #:sb_g2:(f - 0x214)]

	\store	0, c0, [r0, #:pc_g0:(f - 0x214)]
	\store	0, c0, [r0, #:pc_g1:(f - 0x214)]
	\store	0, c0, [r0, #:pc_g2:(f - 0x214)]

	\store	0, c0, [r0, #:sb_g0:(f - 0x214)]
	\store	0, c0, [r0, #:sb_g1:(f - 0x214)]
	\store	0, c0, [r0, #:sb_g2:(f - 0x214)]

	.endm

	ldctest ldc stc
	ldctest ldcl stcl
	ldctest ldc2 stc2
	ldctest ldc2l stc2l

@ FLDS/FSTS

	.fpu	vfp

	.macro vfp_test load store reg

	\load	\reg, [r0, #:pc_g0:(f + 0x214)]
	\load	\reg, [r0, #:pc_g1:(f + 0x214)]
	\load	\reg, [r0, #:pc_g2:(f + 0x214)]

	\load	\reg, [r0, #:sb_g0:(f + 0x214)]
	\load	\reg, [r0, #:sb_g1:(f + 0x214)]
	\load	\reg, [r0, #:sb_g2:(f + 0x214)]

	\store	\reg, [r0, #:pc_g0:(f + 0x214)]
	\store	\reg, [r0, #:pc_g1:(f + 0x214)]
	\store	\reg, [r0, #:pc_g2:(f + 0x214)]

	\store	\reg, [r0, #:sb_g0:(f + 0x214)]
	\store	\reg, [r0, #:sb_g1:(f + 0x214)]
	\store	\reg, [r0, #:sb_g2:(f + 0x214)]

	\load	\reg, [r0, #:pc_g0:(f - 0x214)]
	\load	\reg, [r0, #:pc_g1:(f - 0x214)]
	\load	\reg, [r0, #:pc_g2:(f - 0x214)]

	\load	\reg, [r0, #:sb_g0:(f - 0x214)]
	\load	\reg, [r0, #:sb_g1:(f - 0x214)]
	\load	\reg, [r0, #:sb_g2:(f - 0x214)]

	\store	\reg, [r0, #:pc_g0:(f - 0x214)]
	\store	\reg, [r0, #:pc_g1:(f - 0x214)]
	\store	\reg, [r0, #:pc_g2:(f - 0x214)]

	\store	\reg, [r0, #:sb_g0:(f - 0x214)]
	\store	\reg, [r0, #:sb_g1:(f - 0x214)]
	\store	\reg, [r0, #:sb_g2:(f - 0x214)]

	.endm

	vfp_test flds fsts s0

@ FLDD/FSTD

	vfp_test fldd fstd d0

@ VLDR/VSTR

	vfp_test vldr vstr d0

