	/* sme-fp8-fail.s Test file for error-checking AArch64 SME 8-bit
	floating-point vector instructions.  */

	.macro cvt_pat1, op
	/* Check element width qualifier for source register.  */
	\op	{z0.h-z1.h}, z2.b /* Valid.  */
	\op	{z0.h-z1.h}, z2.h
	\op	{z0.h-z1.h}, z2.s
	\op	{z0.h-z1.h}, z2.d
	/* Check element width qualifier for destination register pair.  */
	\op	{z0.b-z1.b}, z2.b
	\op	{z0.h-z1.h}, z2.b /* Valid.  */
	\op	{z0.s-z1.s}, z2.b
	\op	{z0.d-z1.d}, z2.b
	/* Check whether destination register range starts at even register.  */
	\op	{z1.h-z2.h}, z2.b
	.endm

	cvt_pat1 bf1cvt
	cvt_pat1 bf2cvt
	cvt_pat1 bf1cvtl
	cvt_pat1 bf2cvtl
	cvt_pat1 f1cvt
	cvt_pat1 f2cvt
	cvt_pat1 f1cvtl
	cvt_pat1 f2cvtl

	.macro cvt_pat2, op
	/* Check element width qualifier for destination register.  */
	\op	z2.b, {z0.h-z1.h} /* Valid.  */
	\op	z2.h, {z0.h-z1.h}
	\op	z2.s, {z0.h-z1.h}
	\op	z2.d, {z0.h-z1.h}
	/* Check element width qualifier for source register pair.  */
	\op	z2.b, {z0.b-z1.b}
	\op	z2.b, {z0.h-z1.h} /* Valid.  */
	\op	z2.b, {z0.s-z1.s}
	\op	z2.b, {z0.d-z1.d}
	/* Check whether source register range starts at even register.  */
	\op	z2.b, {z1.h-z2.h}
	.endm

	cvt_pat2 bfcvt
	cvt_pat2 fcvt

	.macro cvt_pat3, op
	/* Check element width qualifier for destination register.  */
	\op	z4.b, {z0.s-z3.s} /* Valid.  */
	\op	z4.h, {z0.s-z3.s}
	\op	z4.s, {z0.s-z3.s}
	\op	z4.d, {z0.s-z3.s}
	/* Check element width qualifier for source register pair.  */
	\op	z4.b, {z0.b-z3.b}
	\op	z4.b, {z0.h-z3.h}
	\op	z4.b, {z0.s-z3.s} /* Valid.  */
	\op	z4.b, {z0.d-z3.d}
	/* Check whether start of source register range is multiple of 4.  */
	\op	z4.b, {z1.s-z4.s}
	\op	z4.b, {z2.s-z5.s}
	\op	z4.b, {z3.s-z6.s}
	.endm

	cvt_pat3 fcvtn
	cvt_pat3 fcvt

	.macro fscale_single, w
	/* pair.  */
	/* Ensure the two multi-vector groups are tied.  */
	fscale { z0.\w - z1.\w }, { z2.\w - z3.\w }, z2.\w
	/* Check first source multi-vector group starts at even register.  */
	fscale { z1.\w - z2.\w }, { z1.\w - z2.\w }, z3.\w
	/* quad.  */
	/* Ensure the two multi-vector groups are tied.  */
	fscale { z0.\w-z3.\w }, { z1.\w-z4.\w }, z4.\w
	/* Check start of first source multi-vector group is multiple of 4.  */
	fscale { z1.\w-z4.\w }, { z1.\w-z4.\w }, z4.\w
	fscale { z2.\w-z5.\w }, { z2.\w-z5.\w }, z4.\w
	fscale { z3.\w-z6.\w }, { z3.\w-z6.\w }, z4.\w
	.endm

	fscale_single h
	fscale_single s
	fscale_single d

	.macro fscale_multi, w
	/* pair.  */
	/* Ensure the two multi-vector groups are tied.  */
	fscale { z0.\w - z1.\w }, { z1.\w - z2.\w }, { z2.\w-z3.\w }
	/* Check multi-vector groups start at even register.  */
	fscale { z1.\w - z2.\w }, { z1.\w - z2.\w }, { z2.\w-z3.\w }
	fscale { z1.\w - z2.\w }, { z1.\w - z2.\w }, { z3.\w-z4.\w }
	/* quad.  */
	/* Ensure the two multi-vector groups are tied.  */
	fscale { z0.\w-z3.\w }, { z1.\w-z4.\w }, { z4.\w-z7.\w }
	/* Check the start of multi-vector group ranges is multiple of 4.  */
	fscale { z1.\w-z4.\w }, { z1.\w-z4.\w }, { z4.\w-z7.\w }
	fscale { z2.\w-z5.\w }, { z2.\w-z5.\w }, { z4.\w-z7.\w }
	fscale { z3.\w-z6.\w }, { z3.\w-z6.\w }, { z4.\w-z7.\w }
	fscale { z0.\w-z3.\w }, { z0.\w-z3.\w }, { z5.\w-z8.\w }
	fscale { z0.\w-z3.\w }, { z0.\w-z3.\w }, { z6.\w-z9.\w }
	fscale { z0.\w-z3.\w }, { z0.\w-z3.\w }, { z7.\w-z10.\w }
	.endm

	fscale_multi h
	fscale_multi s
	fscale_multi d
